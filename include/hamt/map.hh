#ifndef HAMT_MAP_HH
#define HAMT_MAP_HH

#include "hash.hh"
#include "eql.hh"
#include "allocator.hh"
#include <cstddef> 
#include <cstdlib>

namespace hamt {
  typedef unsigned bitmap_t;
  typedef unsigned hashcode_t;
  const unsigned PER_AMT_NODE_BIT_LENGTH = 5;

  namespace {
    // 1bitの数を数える
    unsigned bitcount(unsigned i) {
      i = i - ((i >> 1) & 0x55555555);
      i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
      return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
    }
  }
  
  template <class Key, class Value>
  struct amt_leaf {
    amt_leaf(const Key& key, const Value& value) : key(key), value(value) {}

    Key key;
    Value value;
  };

  template <class Leaf>
  class amt_node {
  public:
    union entry {
      amt_node* node;
      Leaf* leaf;
      unsigned ptrval;
      
      bool is_node() const { return ptrval & 1; }
      bool is_leaf() const { return is_node()==false; }
      bool is_null() const { return leaf == NULL; }

      static entry from_node(amt_node* node) {
        entry e;
        e.node = node;
        e.ptrval++; // 最下位ビットを1にする
        return e;
      }

      amt_node* to_node() {
        entry e = *this;
        e.ptrval--; // 最下位ビットを0に戻す
        return e.node;
      }

      static entry from_leaf(Leaf* leaf) {
        entry e;
        e.leaf = leaf;
        return e;
      }

      Leaf* to_leaf() const { return leaf; }

      static entry null() { return from_leaf(NULL); }
    };

  public:
    amt_node() : entries(NULL),bitmap(0), entries_size(0) {}
    ~amt_node() { 
      free(entries);
    }

    entry get_entry(unsigned index) const {
      if(is_valid_entry(index) == false)
        return entry::null();
      return entries[entry_index(index)];
    }

    entry* get_entry_place(unsigned index) {
      if(is_valid_entry(index) == false)
        set_entry(index, entry::null());
      return &entries[entry_index(index)];      
    }
    
    void set_entry(unsigned index, entry enty) {
      const unsigned e_index = entry_index(index);
      entries = reinterpret_cast<entry*>(realloc(entries, sizeof(entry) * (entries_size+1)));

      for(unsigned i = entries_size; i > e_index; i--) 
        entries[i] = entries[i-1];

      entries[e_index] = enty;
      bitmap |= (1 << index);
      entries_size++;
    }
    
    void init_entries(unsigned index1, entry entry1, unsigned index2, entry entry2) {
      entries = reinterpret_cast<entry*>(malloc(sizeof(entry)*2));
      bitmap |= (1 << index1);
      bitmap |= (1 << index2);
      entries_size = 2;
      
      if(index1 < index2) {
        entries[0] = entry1;
        entries[1] = entry2;
      } else {
        entries[0] = entry2;
        entries[1] = entry1;
      }
    }

    bool is_valid_entry(unsigned index) const {
      return (bitmap >> index) & 1;
    }

    unsigned entry_index(unsigned index) const {
      return bitcount(bitmap & ((1 << index)-1));
    }

  private:
    entry* entries;
    bitmap_t bitmap;
    unsigned char entries_size;
  };

  template<class Key, class Hash>
  class arc_stream {
  public:
    arc_stream(const Key& key) 
      : key(key), rehash_count(0), hashcode(hash(key, rehash_count)), start(0) {}
        
    arc_stream(const Key& key, const arc_stream& o) 
      : key(key), rehash_count(o.rehash_count), hashcode(hash(key, rehash_count)), start(o.start) {}

    unsigned read() {
      return read_n(PER_AMT_NODE_BIT_LENGTH);
    }

    unsigned read_n(unsigned n) {
      if(start > sizeof(hashcode_t)*8) {
        rehash_count++;
        hashcode = hash(key, rehash_count);
        start = 0;
      }

      unsigned mask = (1 << n)-1;
      unsigned arc = (hashcode >> start) & mask;
      start += n;
      return arc;
    }

  private:
    const Key& key;
    unsigned rehash_count;
    hashcode_t hashcode;
    unsigned start;

    static const Hash hash;
  };
  template<class Key, class Hash>
  const Hash arc_stream<Key,Hash>::hash;

  template <class Key, class Value, 
            class Hash=hamt::hash_functor<Key>, class Eql=hamt::eql_functor<Key> >
  class map {
    typedef arc_stream<Key, Hash> arc_stream_t;
    typedef amt_leaf<Key,Value> amt_leaf_t;
    typedef amt_node<amt_leaf_t> amt_node_t;
    typedef typename amt_node_t::entry amt_entry_t;

  public:
    map() : root_entries(NULL), new_root_entries(NULL),
            root_bitlen(3), 
            entries_size(1 << root_bitlen),
            entry_count(0) 
    {
      const unsigned init_size = entries_size;
      resize_border = init_size << PER_AMT_NODE_BIT_LENGTH;
      root_entries = new amt_entry_t[init_size];
      for(unsigned i=0; i < init_size; i++)
        root_entries[i] = amt_entry_t::null();
    }
    
    ~map() {
      delete [] root_entries;
      delete [] new_root_entries;
    }

    Value* find(const Key& key) const {
      arc_stream_t in(key);
      amt_entry_t e = find_impl3(in);
      if(e.is_null() || eql(key, e.to_leaf()->key) == false)
        return NULL;
      return &e.to_leaf()->value;
    }

    unsigned erase(const Key& key) {
      arc_stream_t in(key);
      amt_entry_t* place = find_impl2(in);
      if(place->is_null() == false) {
        entry_count--;
        *place = amt_entry_t::null();
        return 1;
      }
      return 0;
    }

    void set(const Key& key, const Value& value) {
      arc_stream_t in(key);
      amt_entry_t* place = find_impl2(in);
      if(place->is_null()) {
        *place = amt_entry_t::from_leaf(new (alloca.allocate()) amt_leaf_t(key, value));
      } else {
        amt_leaf_t* entry = place->to_leaf();
        
        if(eql(key, entry->key)) {
          entry->value = value;
          return;
        } else {
          arc_stream_t in1(entry->key, in);
          amt_leaf_t* e1 = new amt_leaf_t(key, value);
          
          *place = amt_entry_t::from_node(new amt_node_t);
          resolve_collision(in, e1, in1, entry, place->to_node());
        }
      }
      entry_count++;      
      amortized_resize();
    }

    void amortized_resize() {
      resize_border--;
      if(resize_border == entries_size) {
        unsigned new_size = entries_size<<PER_AMT_NODE_BIT_LENGTH;
        new_root_entries = new amt_entry_t[new_size];
        for(unsigned i = 0; i < new_size; i++)
          new_root_entries[i] = amt_entry_t::null();
      } else if (resize_border < entries_size) {
        unsigned root_arc = resize_border;
        amt_entry_t val = root_entries[root_arc];
        unsigned new_root_bitlen = root_bitlen + PER_AMT_NODE_BIT_LENGTH;
        if(val.is_leaf()) {
          amt_leaf_t* entry = val.to_leaf();
          arc_stream_t in(entry->key);
          new_root_entries[in.read_n(new_root_bitlen)] = val;
        } else {
          amt_node_t* node = val.to_node();
          for(unsigned i=0; i < 32; i++) { // XXX:
            if(node->is_valid_entry(i)) {
              new_root_entries[root_arc + (i << root_bitlen)] = node->get_entry(i);
            }
          }
          delete node;
        }
        
        root_entries[resize_border] = amt_entry_t::null();
        if(resize_border == 0) {
          root_entries = new_root_entries;
          root_bitlen = new_root_bitlen;
          entries_size <<= PER_AMT_NODE_BIT_LENGTH;
          resize_border = entries_size << PER_AMT_NODE_BIT_LENGTH;
          new_root_entries = NULL;
        }
      }
    }

    void resolve_collision(arc_stream_t& in1, amt_leaf_t* e1,
                           arc_stream_t& in2, amt_leaf_t* e2, amt_node_t* node) {
      for(;;) {
        unsigned arc1 = in1.read();
        unsigned arc2 = in2.read();
        if(arc1 != arc2) {
          node->init_entries(arc1, amt_entry_t::from_leaf(e1), arc2, amt_entry_t::from_leaf(e2));
          break;
        }        
        
        amt_node_t* new_node = new amt_node_t;
        node->set_entry(arc1, amt_entry_t::from_node(new_node));
        node = new_node;
      }
    }

    unsigned size() const { return entry_count; }

  private:
    amt_entry_t* find_impl2(arc_stream_t& in) const {
      amt_entry_t* place;
      unsigned arc = in.read_n(root_bitlen);

      if(arc < resize_border) {
        place = &root_entries[arc];
      } else {
        arc += (in.read() << root_bitlen);
        place = &new_root_entries[arc];
      }

      while(!(place->is_null() || place->is_leaf())) {
        arc = in.read();
        place = place->to_node()->get_entry_place(arc);
      }  

      return place;
    }

    amt_entry_t find_impl3(arc_stream_t& in) const {
      amt_entry_t place;
      unsigned arc = in.read_n(root_bitlen);

      if(arc < resize_border) {
        place = root_entries[arc];
      } else {
        arc += (in.read() << root_bitlen);
        place = new_root_entries[arc];
      }

      while(!(place.is_null() || place.is_leaf())) {
        arc = in.read();
        place = place.to_node()->get_entry(arc);
      }  

      return place;
    }

  private:
    amt_entry_t* root_entries;
    amt_entry_t* new_root_entries;
    unsigned root_bitlen;
    unsigned entries_size;
    unsigned resize_border;
    unsigned entry_count;

    fixed_size_allocator<sizeof(amt_leaf_t)> alloca;
    static const Eql eql;
  };

  template<class Key, class Value, class Hash, class Eql>
  const Eql map<Key,Value,Hash,Eql>::eql;
}

#endif
