#include "hamt/map.hh"
#include <iostream>
#include <cstdlib>

#include <tr1/unordered_map>

using namespace std;
using namespace tr1;

#define DSIZE 1000000
#define N 10

#include <sys/time.h>
inline double gettime(){
  timeval tv;
  gettimeofday(&tv,NULL);
  return static_cast<double>(tv.tv_sec)+static_cast<double>(tv.tv_usec)/1000000.0;
}

void hamt_bench(int data1[DSIZE], int data2[DSIZE]) {
  hamt::map<int,int> dic;
  double beg_t = gettime();
  int count = 0;

  for(int i=0; i < DSIZE; i++) {
    dic.set(data1[i], data1[i]);
  }
  std::cout << " put:" << gettime()-beg_t << " #" << dic.size() << std::endl;

  beg_t = gettime();
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    if(dic.find(data1[i]))
      count++;
  }
  std::cout << " get1: " << gettime()-beg_t << " #" << count << std::endl;
  
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    if(dic.find(data2[i]))
      count++;
  }
  std::cout << " get2: " << gettime()-beg_t << " #" << count << std::endl;

  beg_t = gettime();
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    count += dic.erase(data1[i]);
  }
  std::cout << " erase: " << gettime()-beg_t << " #" << count << std::endl;  

  beg_t = gettime();
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    if(dic.find(data2[i]))
      count++;
  }
  std::cout << " get3: " << gettime()-beg_t << " #" << count << std::endl;

}

void hamt_bench_small(int data1[DSIZE], int data2[DSIZE]) {
  double beg_t = gettime();
  unsigned count = 0;

  for(int i=0; i < DSIZE; i += N) {
    hamt::map<int,int> dic;  
    
    for(int j=0; j < N; j++)
      dic.set(data1[i+j], data1[i+j]);
    for(int j=0; j < N; j++)
      if(dic.find(data1[i+j]))
        count++;
    for(int j=0; j < N; j++)
      if(dic.find(data2[i+j]))
        count++;
  }

  std::cout << " put, get1, get2: " << gettime()-beg_t << " #" << count << std::endl;
}

void unorderedmap_bench_small(int data1[DSIZE], int data2[DSIZE]) {
  double beg_t = gettime();
  unsigned count = 0;

  for(int i=0; i < DSIZE; i += N) {
    unordered_map<int,int> dic;

    for(int j=0; j < N; j++)
      dic[data1[i+j]] = data1[i+j];
    for(int j=0; j < N; j++)
      if(dic.find(data1[i+j]) != dic.end())
        count++;
    for(int j=0; j < N; j++)
      if(dic.find(data2[i+j]) != dic.end())
        count++;
  }
  std::cout << " put, get1, get2: " << gettime()-beg_t << " #" << count << std::endl;
}

void unorderedmap_bench(int data1[DSIZE], int data2[DSIZE]) {
  unordered_map<int,int> dic;
  double beg_t = gettime();
  int count = 0;

  for(int i=0; i < DSIZE; i++) {
    dic[data1[i]] = data1[i];
  }
  std::cout << " put: " << gettime()-beg_t << " #" << dic.size() << std::endl;

  beg_t = gettime();
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    if(dic.find(data1[i]) != dic.end())
      count++;
  }
  std::cout << " get1: " << gettime()-beg_t << " #" << count << std::endl;
  
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    if(dic.find(data2[i]) != dic.end())
      count++;
  }
  std::cout << " get2: " << gettime()-beg_t << " #" << count << std::endl;  

  beg_t = gettime();
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    count += dic.erase(data1[i]);
  }
  std::cout << " erase: " << gettime()-beg_t << " #" << count << std::endl;  

  beg_t = gettime();
  count = 0;
  for(int i=0; i < DSIZE; i++) {
    if(dic.find(data2[i]) != dic.end())
      count++;
  }
  std::cout << " get3: " << gettime()-beg_t << " #" << count << std::endl;  
}

int main(int argc, char** argv) {
  int data1[DSIZE];
  int data2[DSIZE];

  for(int i=0; i < DSIZE; i++) {
    data1[i] = rand() % (DSIZE*5);
    data2[i] = rand() % (DSIZE*5);
  }

  double beg_t = gettime();
  std::cout << "hamt: large" << std::endl;
  hamt_bench(data1, data2);
  std::cout << " => total: " << gettime()-beg_t << std::endl << std::endl;;

  beg_t = gettime();
  std::cout << "unorderedmap: large" << std::endl;
  unorderedmap_bench(data1, data2);
  std::cout << " => total: " << gettime()-beg_t << std::endl << std::endl;;  

  beg_t = gettime();
  std::cout << "hamt: small" << std::endl;
  hamt_bench_small(data1, data2);
  std::cout << " => total: " << gettime()-beg_t << std::endl << std::endl;;

  beg_t = gettime();
  std::cout << "unorderedmap: small" << std::endl;
  unorderedmap_bench_small(data1, data2);
  std::cout << " => total: " << gettime()-beg_t << std::endl << std::endl;;

  return 0;
}
