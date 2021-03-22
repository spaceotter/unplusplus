#include "test.hh"

int Foo::pashoo(float z) {
  b += z/2;
  return b;
}

void Bar::store() {
  d = *c;
}

double Bar::meep(void *x) {
  return *(double *)x * 2;
}

template<>
int C_int::meep(int x) {
  return x+2;
}
