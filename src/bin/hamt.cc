#include "hamt/map.hh"
#include <iostream>

int main(int argc, char** argv) {
  int keys[100] = {1, 33, 20, 2, 1111, 35, 54, 55};
  for(int i=0; i < 100; i++) {
    keys[i] = i;
  }
  hamt::map<int,int> m;
  std::cout << "# insert" << std::endl;
  for(unsigned i=0; i < sizeof(keys)/sizeof(int); i++) {
    std::cout << keys[i] << " = " << keys[i] << std::endl;
    m.set(keys[i], keys[i]);
  }

  std::cout << std::endl << "# find" << std::endl;
  for(unsigned i=0; i < sizeof(keys)/sizeof(int); i++) {
    std::cout << keys[i] << " = " << (m.find(keys[i]) ? *m.find(keys[i]) : 0) << std::endl;
  }
  return 0;
}
