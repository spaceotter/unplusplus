#include <stdint.h>

#define TEST_STR "{has braces}"

struct Foo {
  int8_t a;
  uint16_t b;
  int32_t *c;
  virtual void store() = 0;
  int pashoo(float z);
};

struct Bar : public Foo {
  uint64_t d;
  virtual void store();
  double meep(void *x);
};

void something(Bar *x);

Bar *alternately(Foo *z, int yams);

template<typename T>
class C { T t;
  T meep(T x) {return x+1;} };

typedef class C<int> C_int;
