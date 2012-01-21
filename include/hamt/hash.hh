#ifndef HAMT_HASH_HH
#define HAMT_HASH_HH

#include <string>
#include <cassert>

namespace hamt {
  const unsigned GOLDEN_RATIO_PRIME=0x9e370001; //(2^31) + (2^29) - (2^25) + (2^22) - (2^19) - (2^16) + 1;

  template<class Key>
  class hash_functor {
  public:
    hash_functor() {}

    unsigned operator()(const Key& key, unsigned n) const {
      assert(n == 0); // XXX: 複数のハッシュ関数には未対応
      return hash(key, n); 
    }
  };

  unsigned hash(unsigned key, unsigned n) { return key * GOLDEN_RATIO_PRIME; }
  unsigned hash(int key, unsigned n) { return hash(static_cast<unsigned>(key), n); }
  unsigned hash(long key, unsigned n) { return hash(static_cast<unsigned>(key), n ); }
  unsigned hash(const char* key, unsigned n) { return hash(reinterpret_cast<long>(key), n); }

  unsigned hash(const std::string& key, unsigned n) {
    unsigned h = GOLDEN_RATIO_PRIME;
    for(unsigned i=0; i < n; i++)  // XXX: 適当
      for(const char* c=key.c_str(); *c != 0; c++)
        h = (h*33) + *c;
    return h;
  }
}

#endif
