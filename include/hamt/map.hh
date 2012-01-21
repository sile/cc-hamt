#ifndef HAMT_MAP_HH
#define HAMT_MAP_HH

#include "hash.hh"
#include "eql.hh"
#include "allocator.hh"
#include <cstddef> // for NULL

// for debug
#include <iostream>
#include <cassert>

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
  struct entry { // => amt_leaf?
    entry(const Key& key, const Value& value) : key(key), value(value) {}

    Key key;
    Value value;
  };

  template <class Entry>
  struct amt_node {
    union value {
      amt_node* node;
      Entry* entry;
      unsigned ptrval;

      bool is_node() const { return ptrval & 1; }
      bool is_entry() const { return is_node()==false; }
      bool is_null() const { return entry == NULL; }

      static value from_node(amt_node* node) {
        value v;
        v.node = node;
        v.ptrval++; // 最下位ビットを1にする
        return v;
      }

      static value from_entry(Entry* entry) {
        value v;
        v.entry = entry;
        return v;
      }

      static value null() {
        return from_entry(NULL);
      }
      
      amt_node* to_node() {
        value v = *this;
        v.ptrval--;// 最下位ビットを0に戻す
        return v.node;
      }

      Entry* to_entry() const {
        return entry;
      }
      
    };
    
    value* entries;
    bitmap_t bitmap;
    unsigned char entries_size;
    
    amt_node() : entries(NULL),bitmap(0), entries_size(0) {
    }
    ~amt_node() {
      delete [] entries;
    }

    bool is_valid_entry(unsigned index) const {
      return (bitmap >> index) & 1;
    }

    value get_entry(unsigned index) const {
      if(is_valid_entry(index) == false)
        return value::null();
      return entries[entry_index(index)];
    }

    value* get_entry_place(unsigned index) {
      if(is_valid_entry(index) == false) {
        set_entry(index, value::null());
      }
      return &entries[entry_index(index)];      
    }
    
    unsigned entry_index(unsigned index) const {
      return bitcount(bitmap & ((1 << index)-1));
    }
    
    void set_entry(unsigned index, value entry) {
      const unsigned e_index = entry_index(index);
      entries = (value*)realloc(entries, sizeof(value) * (entries_size+1));
      for(unsigned i = entries_size; i > e_index; i--) {
        entries[i] = entries[i-1];
      }
      entries[e_index] = entry;
      bitmap |= (1 << index);
      entries_size++;
    }
    
    void init_entries(unsigned index1, value entry1, unsigned index2, value entry2) {
      entries = new value[2];
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
  };

  template<class Key, class Hash>
  struct arc_stream {
    const Key& key;
    hashcode_t hashcode;
    unsigned start;
    unsigned rehash_count;

    arc_stream(const Key& key) : key(key), hashcode(hash(key)), start(0),
                                 rehash_count(1) {}
    arc_stream(const Key& key, const arc_stream& o) :
      key(key), hashcode(hash(key, o.rehash_count)), start(o.start), rehash_count(o.rehash_count) {
    }
    
    unsigned read() {
      return read_n(PER_AMT_NODE_BIT_LENGTH);
    }

    unsigned read_n(unsigned n) {
      if(start >= sizeof(hashcode_t)*8) {
        rehash_count++;
        hashcode = hash(key, rehash_count);
        start = 0;
      }

      unsigned mask = (1 << n)-1;
      unsigned arc = (hashcode >> start) & mask;
      start += n;
      return arc;
    }

    static const Hash hash;
  };
  template<class Key, class Hash>
  const Hash arc_stream<Key,Hash>::hash;

  template <class Key, class Value, 
            class Hash=hamt::hash_functor<Key>, class Eql=hamt::eql_functor<Key> >
  class map {
    typedef entry<Key,Value> entry_t;
    typedef arc_stream<Key, Hash> arc_stream_t;
    typedef amt_node<entry_t> amt_node_t;
    typedef typename amt_node_t::value amt_val_t;

  public:
    map() : root_entries(NULL), new_root_entries(NULL),
            e_size(1 << 3),
            root_bitlen(3), 
            entry_count(0)
    {
      const unsigned init_size = e_size;
      resize_border = init_size << PER_AMT_NODE_BIT_LENGTH;
      root_entries = new amt_val_t[init_size];
      for(unsigned i=0; i < init_size; i++)
        root_entries[i] = amt_val_t::null();
    }
    
    ~map() {
      delete [] root_entries;
      delete [] new_root_entries;
    }

    Value* find(const Key& key) const {
      arc_stream_t in(key);
      amt_val_t e = find_impl3(in);
      if(e.is_null() || eql(key, e.to_entry()->key) == false)
        return NULL;
      return &e.to_entry()->value;
    }

    unsigned erase(const Key& key) {
      arc_stream_t in(key);
      amt_val_t* place = find_impl2(in);
      if(place->is_null() == false) {
        entry_count--;
        *place = amt_val_t::null();
        return 1;
      }
      return 0;
    }

    void set(const Key& key, const Value& value) {
      arc_stream_t in(key);
      amt_val_t* place = find_impl2(in);
      if(place->is_null()) {
        entry_count++;
        *place = amt_val_t::from_entry(new (alloca.allocate()) entry_t(key, value));
        amortized_resize();
      } else {
        entry_t* entry = place->to_entry();
        
        if(eql(key, entry->key)) {
          entry->value = value;
        } else {
          entry_count++;
          arc_stream_t in1(entry->key, in);
          entry_t* e1 = new entry_t(key, value);

          *place = amt_val_t::from_node(new amt_node_t);
          resolve_collision(in, e1, in1, entry, place->to_node());
          amortized_resize();
        }
      }
    }

    void amortized_resize() {
      resize_border--;
      if(resize_border == e_size) {
        unsigned new_size = e_size<<PER_AMT_NODE_BIT_LENGTH;
        new_root_entries = new amt_val_t[new_size];
        for(unsigned i = 0; i < new_size; i++)
          new_root_entries[i] = amt_val_t::null();
      } else if (resize_border < e_size) {
        unsigned root_arc = resize_border;
        amt_val_t val = root_entries[root_arc];
        unsigned new_root_bitlen = root_bitlen + PER_AMT_NODE_BIT_LENGTH;
        if(val.is_entry()) {
          entry_t* entry = val.to_entry();
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
        
        root_entries[resize_border] = amt_val_t::null();
        if(resize_border == 0) {
          root_entries = new_root_entries;
          root_bitlen = new_root_bitlen;
          e_size <<= PER_AMT_NODE_BIT_LENGTH;
          resize_border = e_size << PER_AMT_NODE_BIT_LENGTH;
          new_root_entries = NULL;
        }
      }
    }

    void resolve_collision(arc_stream_t& in1, entry_t* e1,
                           arc_stream_t& in2, entry_t* e2, amt_node_t* node) {
      for(;;) {
        unsigned arc1 = in1.read();
        unsigned arc2 = in2.read();
        if(arc1 != arc2) {
          node->init_entries(arc1, amt_val_t::from_entry(e1), arc2, amt_val_t::from_entry(e2));
          break;
        }        
        
        amt_node_t* new_node = new amt_node_t;
        node->set_entry(arc1, amt_val_t::from_node(new_node));
        node = new_node;
      }
    }

    unsigned size() const { return entry_count; }

  private:
    amt_val_t* find_impl2(arc_stream_t& in) const {
      amt_val_t* place;
      unsigned arc = in.read_n(root_bitlen);

      if(arc < resize_border) {
        place = &root_entries[arc];
      } else {
        arc += (in.read() << root_bitlen);
        place = &new_root_entries[arc];
      }

      while(!(place->is_null() || place->is_entry())) {
        arc = in.read();
        place = place->to_node()->get_entry_place(arc);
      }  

      return place;
    }

    amt_val_t find_impl3(arc_stream_t& in) const {
      amt_val_t place;
      unsigned arc = in.read_n(root_bitlen);

      if(arc < resize_border) {
        place = root_entries[arc];
      } else {
        arc += (in.read() << root_bitlen);
        place = new_root_entries[arc];
      }

      while(!(place.is_null() || place.is_entry())) {
        arc = in.read();
        place = place.to_node()->get_entry(arc);
      }  

      return place;
    }

  private:
    amt_val_t* root_entries;
    amt_val_t* new_root_entries;
    unsigned e_size;
    unsigned root_bitlen;
    unsigned resize_border;
    unsigned entry_count;

    static const Hash hash;
    static const Eql eql;

    fixed_size_allocator<sizeof(entry_t)> alloca;
  };

  template<class Key, class Value, class Hash, class Eql>
  const Hash map<Key,Value,Hash,Eql>::hash;

  template<class Key, class Value, class Hash, class Eql>
  const Eql map<Key,Value,Hash,Eql>::eql;
}

#endif
