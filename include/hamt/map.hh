#ifndef HAMT_MAP_HH
#define HAMT_MAP_HH

#include "hamt/hash.hh"
#include "hamt/eql.hh"
#include <cstddef> // for NULL

// for debug
#include <iostream>

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
  
  template <class Key, class Value>
  struct entry {
    hashcode_t hashcode;
    Key key;
    Value value;
  };

  template <class Entry>
  struct amt_node {
    Entry* entries;
    unsigned entries_size;
    bitmap_t bitmap;
    
    amt_node() : entries(NULL), entries_size(0), bitmap(0) {}
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
        Entry* new_entries = new Entry[entries_size+1]; // TODO: allocator

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

  template <class Key, class Value, 
            class Hash=hamt::hash_functor<Key>, class Eql=hamt::eql_functor<Key> >
  class map {
    typedef entry<Key,Value> entry_t;
  public:
    map() : root_entries(NULL), new_root_entries(NULL),
            root_bitlen(3), 
            entry_count(0)
    {
      const unsigned init_size = 1 << root_bitlen;
      resize_border = init_size << PER_ARC_BIT_LENGTH;
      root_entries = new entry_t[init_size];
    }
    
    ~map() {
      delete [] root_entries;
      delete [] new_root_entries;
    }
      
    Value* find(const Key& key) const {
      return NULL;
    }

  private:
    entry_t* root_entries;
    entry_t* new_root_entries;
    unsigned root_bitlen;
    unsigned resize_border;
    unsigned entry_count;

    static const Hash hash;
    static const Eql eql;
  };
}

#endif
