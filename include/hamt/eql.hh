#ifndef HAMT_EQL_HH
#define HAMT_EQL_HH

#include <string>

namespace hamt {
  template<class Key>
  class eql_functor {
  public:
    eql_functor() {}

    bool operator()(const Key& k1, const Key& k2) const {
      return eql(k1, k2);
    }
  };
  
  template<class T>
  inline bool eql(T k1, T k2) {
    return k1 == k2;
  }
}

#endif
