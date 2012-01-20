/**
 * @license cc-dict 0.0.7
 * Copyright (c) 2011, Takeru Ohta, phjgt308@gmail.com
 * MIT license
 * https://github.com/sile/cc-hamt/blob/master/COPYING
 */
#ifndef HAMT_ALLOCATOR_HH
#define HAMT_ALLOCATOR_HH

#include <cstdlib>

namespace hamt {
  template<unsigned CHUNK_SIZE>
  class fixed_size_allocator { 
  private:
    static const unsigned INITIAL_BLOCK_SIZE=8;

    struct chunk {
      union {
        char buf[CHUNK_SIZE];
        chunk* free_next;
      };
    };
    
    struct chunk_block {
      chunk* chunks;
      unsigned size;
      chunk_block* prev;
      
      chunk_block(unsigned size) : chunks(NULL), size(size), prev(NULL) {
        chunks = new chunk[size];
      }
      
      ~chunk_block() {
        delete [] chunks;
        delete prev;
      }
    };

  public:
    fixed_size_allocator()
      : block(NULL), position(0), recycle_count(0) {
      block = new chunk_block(INITIAL_BLOCK_SIZE);
    }
      
    ~fixed_size_allocator() {
      clear();
      delete block;
    }
    
    void* allocate() {
      chunk* p;
      
      if(recycle_count > 0) {
        chunk* head = peek_chunk();
        p = head->free_next;
        head->free_next = p->free_next;

        recycle_count--;
      } else {
        p = read_chunk();
      }
      
      return p;
    }

    void release(void* ptr) {
      chunk* free = reinterpret_cast<chunk*>(ptr);
      chunk* head = peek_chunk();
      free->free_next = head->free_next;
      head->free_next = free;
      
      recycle_count++;
    }

    void clear() {
      delete block->prev;
      block->prev = NULL;
      position = 0;
      recycle_count = 0;
    }

  private:
    chunk* read_chunk() {
      chunk* p = peek_chunk();
      if(++position == block->size)
        enlarge();      
      return p;
    }
    
    chunk* peek_chunk() const {
      return block->chunks + position;
    }
    
    void enlarge () {
      chunk_block* new_block = new chunk_block(block->size*1.25);
      new_block->prev = block;
      block = new_block;
      position = 0;
    }

  protected:
    chunk_block* block;
    unsigned position;
    unsigned recycle_count;
  };
}

#endif
