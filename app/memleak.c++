#include <iostream>

int main(int argc, char *argv[]) {
  int t = 0;
  for (int i=0; i<1000; i++)
  {
    // OK
    int * p = new int;
    *p = i + argc;

    // Memory leak
    int * q = new int;

    *q = *p + t;
    t += *q;
    delete p; 
  }
  std::cout<<"tot: " << t << std::endl;
}
