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
    unsigned bitcount(unsigned n) {
      n = (n & 0x55555555) + (n >> 1 & 0x55555555);
      n = (n & 0x33333333) + (n >> 2 & 0x33333333);
      n = (n & 0x0f0f0f0f) + (n >> 4 & 0x0f0f0f0f);
      n = (n & 0x00ff00ff) + (n >> 8 & 0x00ff00ff);
      return (n & 0x0000ffff) + (n >>16 & 0x0000ffff);
    }
  }

  // XXX:
  enum E_TYPE {
    E_ENTRY,
    E_NODE
  };

  template <class Key, class Value>
  struct entry {
    entry() : type(E_ENTRY) {}

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
    
    amt_node() : type(E_NODE), entries(NULL), entries_size(0), bitmap(0) {}
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
    
    unsigned entry_index(unsigned index) const {
      return bitcount(bitmap & ((2 << index)-1));
    }
    
    void set_entry(unsigned index, Entry* entry) {
      const unsigned e_index = entry_index(index);
      if(is_valid_entry(index)) {
        entries[e_index] = entry;
      } else {
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
      entries = new Entry[2]; // TODO: allocator
      bitmap |= (1 << index1);
      bitmap |= (1 << index2);
      
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

    arc_stream(const Key& key) : key(key), hashcode(hash(key)), start(0) {}
    
    unsigned read() {
      return read_n(PER_ARC_BIT_LENGTH);
    }

    unsigned read_n(unsigned n) {
      assert(start < sizeof(hashcode_t)*8);
      unsigned arc = (hashcode >> start) & ((2 << n)-1);
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
            root_bitlen(3), 
            entry_count(0)
    {
      const unsigned init_size = 1 << root_bitlen;
      resize_border = init_size << PER_ARC_BIT_LENGTH;
      root_entries = new entry_t*[init_size];
    }
    
    ~map() {
      delete [] root_entries;
      delete [] new_root_entries;
    }

    Value* find(const Key& key) const {
      arc_stream_t in(key);
      entry_t* entry = find_impl(in);
      if(entry == NULL || eql(key, entry->key) == false)
        return NULL;
      return &entry->value;
    }

    void set(const Key& key, const Value& value) {
      arc_stream_t in(key);
      entry_t* entry = find_impl(in);
      if(entry == NULL) {
        entry_count++;
        arc_stream_t in2(key);
        set_if_not_exists_impl(in2, key, value);
        // TODO: amortized_resize()
      } else {
        // TODO: maybe collision
        entry->value = value;
      }
    }

    unsigned size() const { return entry_count; }

  private:
    entry_t* find_impl(arc_stream_t& in) const {
      unsigned arc = in.read_n(root_bitlen);
      entry_t **entries;
      if(arc < resize_border) {
        entries = root_entries;
      } else {
        arc += (in.read() << root_bitlen);
        entries = new_root_entries;
      }
      entry_t* entry = entries[arc];

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

    void set_if_not_exists_impl(arc_stream_t& in, const Key& key, const Value& value) const {
      unsigned arc = in.read_n(root_bitlen);
      entry_t **entries;
      if(arc < resize_border) {
        entries = root_entries;
      } else {
        arc += (in.read() << root_bitlen);
        entries = new_root_entries;
      }
      entry_t* entry = entries[arc];

      if(entry==NULL) {
        entries[arc] = new entry_t(); // XXX:
        entries[arc]->key = key;
        entries[arc]->value = value;
      } else if(entry->type == E_ENTRY) {
        return;
      } else {
        for(;;) {
          amt_node_t* node = (amt_node_t*)entry; // XXX:
          arc = in.read();
          entry = node->get_entry(arc);
          if(entry==NULL) {
            entry_t* e = new entry_t(); // XXX:
            e->key = key;
            e->value = value;
            node->set_entry(arc, e);
          } else if(entry->type == E_ENTRY) {
            return;
          } 
        }
      }      
    }
  private:
    entry_t** root_entries;
    entry_t** new_root_entries;
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
