#ifndef HAMT_MAP_HH
#define HAMT_MAP_HH

#include "hamt/hash.hh"
#include "hamt/eql.hh"
#include <cstddef> // for NULL

// for debug
#include <iostream>
#include <cassert>

namespace hamt {
  typedef unsigned bitmap_t;
  typedef unsigned hashcode_t;
  const unsigned PER_ARC_BIT_LENGTH = 5;

  namespace {
    unsigned bitcount(unsigned i)
    {
      i = i - ((i >> 1) & 0x55555555);
      i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
      return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
    }
    
    /*
    unsigned bitcount2(unsigned n) {
      n = (n & 0x55555555) + (n >> 1 & 0x55555555);
      n = (n & 0x33333333) + (n >> 2 & 0x33333333);
      n = (n & 0x0f0f0f0f) + (n >> 4 & 0x0f0f0f0f);
      n = (n & 0x00ff00ff) + (n >> 8 & 0x00ff00ff);
      n = (n & 0x0000ffff) + (n >>16 & 0x0000ffff);
      return n;
    }
    */
  }
 
  // XXX:
  enum E_TYPE {
    E_ENTRY,
    E_NODE
  };

  template <class Key, class Value>
  struct entry {
    entry() : type(E_ENTRY) {}
    entry(const Key& key, const Value& value) : type(E_ENTRY), key(key), value(value) {}

    E_TYPE type;
    Key key;
    Value value;
  };

  template <class Entry>
  struct amt_node {
    E_TYPE type;
    Entry** entries;
    unsigned entries_size;
    bitmap_t bitmap;
    
    amt_node() : type(E_NODE), entries(NULL), entries_size(0), bitmap(0) {
    }
    ~amt_node() {
      delete [] entries; // TODO: allocator
    }

    bool is_valid_entry(unsigned index) const {
      return (bitmap >> index) & 1;
    }

    Entry* get_entry(unsigned index) const {
      if(is_valid_entry(index) == false)
        return NULL;
      return entries[entry_index(index)];
    }

    Entry** get_entry_place(unsigned index, bool add) {
      if(is_valid_entry(index) == false) {
        if(add == false) {
          return NULL;
        } else {
          assert(type==E_NODE);
          set_entry(index, NULL);
        }
      }
      return &entries[entry_index(index)];      
    }
    
    unsigned entry_index(unsigned index) const {
      return bitcount(bitmap & ((1 << index)-1));
    }
    
    void set_entry(unsigned index, Entry* entry) {
      const unsigned e_index = entry_index(index);
      if(is_valid_entry(index)) {
        assert(entry->type <= E_NODE);
        entries[e_index] = entry;
      } else {
        assert(type==E_NODE);
        Entry** new_entries = new Entry*[entries_size+1]; // TODO: allocator
        // copy
        unsigned i=0;
        for(; i < e_index; i++) {
          new_entries[i] = entries[i];
        }
        new_entries[i] = entry;
        for(; i < entries_size; i++) {
          new_entries[i+1] = entries[i];
        }
        delete [] entries; // TODO: allocator

        bitmap |= (1 << index);
        entries_size++;
        entries = new_entries;
      }
    }

    void init_entries(unsigned index1, Entry* entry1, unsigned index2, Entry* entry2) {
      entries = new Entry*[2]; // TODO: allocator
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

  // TODO: hashcode2 ...
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
      return read_n(PER_ARC_BIT_LENGTH);
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

  public:
    map() : root_entries(NULL), new_root_entries(NULL),
            e_size(1 << 3),
            root_bitlen(3), 
            entry_count(0)
    {
      const unsigned init_size = 1 << root_bitlen;
      resize_border = init_size << PER_ARC_BIT_LENGTH;
      root_entries = new entry_t*[init_size];
      for(unsigned i=0; i < init_size; i++)
        root_entries[i] = NULL;
    }
    
    ~map() {
      delete [] root_entries;
      delete [] new_root_entries;
    }

    Value* find(const Key& key) const {
      arc_stream_t in(key);
      entry_t* entry = find_impl3(in);
      if(entry == NULL || eql(key, entry->key) == false)
        return NULL;
      return &entry->value;
    }

    /*
    Value& operator[](const Key& key) {
      Value* val = find(key);
      if(val)
        return *val;
      Value v;
      set(key, v);
      return *find(key);
    }
    */

    unsigned erase(const Key& key) {
      return 0;
    }

    void set(const Key& key, const Value& value) {
      arc_stream_t in(key);
      entry_t** place = find_impl2(in, true);
      assert(place);
      entry_t* entry = *place;
      
      if(entry == NULL) {
        entry_count++;
        *place = new entry_t(key, value);
        amortized_resize();
      } else if(entry->type == E_ENTRY) {
        if(eql(key, entry->key)) {
          entry->value = value;
        } else {
          entry_count++;
          arc_stream_t in1(entry->key, in);
          entry_t* e1 = new entry_t(key, value);
          *place = (entry_t*)new amt_node_t;
          resolve_collision(in, e1, in1, entry, (amt_node_t*)*place);          
          amortized_resize();
        }
      } else {
        assert(false);
      }
    }

    void amortized_resize() {
      resize_border--;
      if(resize_border == e_size) {
        unsigned new_size = e_size<<PER_ARC_BIT_LENGTH;
        new_root_entries = new entry_t*[new_size];
        for(unsigned i = 0; i < new_size; i++)
          new_root_entries[i] = NULL;
      } else if (resize_border < e_size) {
        unsigned root_arc = resize_border;
        entry_t *entry = root_entries[root_arc];
        unsigned new_root_bitlen = root_bitlen + PER_ARC_BIT_LENGTH;
        if(entry->type == E_ENTRY) {
          arc_stream_t in(entry->key);
          new_root_entries[in.read_n(new_root_bitlen)] = entry;
          delete entry;
        } else {
          amt_node_t* node = (amt_node_t*)entry;
          for(unsigned i=0; i < 32; i++) { // XXX:
            if(node->is_valid_entry(i)) {
              entry_t* sub = node->get_entry(i);
              new_root_entries[root_arc + (i << root_bitlen)] = sub;
            }
          }
        }
        root_entries[resize_border] = NULL;
        if(resize_border == 0) {
          root_entries = new_root_entries;
          root_bitlen = new_root_bitlen;
          e_size <<= PER_ARC_BIT_LENGTH;
          resize_border = e_size << PER_ARC_BIT_LENGTH;
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
          node->init_entries(arc1, e1, arc2, e2);
          break;
        }        
        
        amt_node_t* new_node = new amt_node_t;
        assert(node->type==E_NODE);
        node->set_entry(arc1, (entry_t*)new_node);
        node = new_node;
      }
    }

    unsigned size() const { return entry_count; }

  private:
    entry_t** find_impl2(arc_stream_t& in) const {
      return find_impl2(in, false);
    }
    
    entry_t** find_impl2(arc_stream_t& in, bool add) const {
      entry_t **place;
      unsigned arc = in.read_n(root_bitlen);

      if(arc < resize_border) {
        place = &root_entries[arc];
      } else {
        arc += (in.read() << root_bitlen);
        place = &new_root_entries[arc];
      }
      entry_t* entry = *place;

      if(entry==NULL) {
        return place;
      } else if(entry->type == E_ENTRY) {
        return place;
      } else {
        for(;;) {
          amt_node_t* node = (amt_node_t*)entry; // XXX:
          arc = in.read();
          place = node->get_entry_place(arc, add);
          if(place==NULL)
            return NULL;
          entry = *place;
          if(entry==NULL) {
            return place;
          } else if(entry->type == E_ENTRY) {
            return place;
          } 
        }
      }      
    }

    entry_t* find_impl3(arc_stream_t& in) const {
      unsigned arc = in.read_n(root_bitlen);
      entry_t* entry;
      
      if(arc < resize_border) {
        entry = root_entries[arc];
      } else {
        arc += (in.read() << root_bitlen);
        entry = new_root_entries[arc];
      }
      
      if(entry==NULL) {
        return NULL;
      } else if(entry->type == E_ENTRY) {
        return entry;
      } else {
        for(;;) {
          amt_node_t* node = (amt_node_t*)entry; // XXX:
          arc = in.read();
          entry = node->get_entry(arc);
          if(entry==NULL) {
            return NULL;
          } else if(entry->type == E_ENTRY) {
            return entry;
          } 
        }
      }
    }
    
  private:
    entry_t** root_entries;
    entry_t** new_root_entries;
    unsigned e_size;
    unsigned root_bitlen;
    unsigned resize_border;
    unsigned entry_count;

    static const Hash hash;
    static const Eql eql;
  };

  template<class Key, class Value, class Hash, class Eql>
  const Hash map<Key,Value,Hash,Eql>::hash;

  template<class Key, class Value, class Hash, class Eql>
  const Eql map<Key,Value,Hash,Eql>::eql;
}

#endif
