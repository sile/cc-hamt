#include "hamt/map.hh"
#include <iostream>

int main(int argc, char** argv) {
  hamt::map<int,int> m;
  m.find(10);
  return 0;
}
