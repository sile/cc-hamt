#include "hamt/map.hh"
#include <iostream>

int main(int argc, char** argv) {
  hamt::map<int,int> m;
  m.set(10, 2);
  m.set(434, 20);
  std::cout << "# " << (m.find(10) ? *m.find(10) : 0) << std::endl;
  std::cout << "# " << (m.find(434) ? *m.find(434) : 0) << std::endl;
  return 0;
}
