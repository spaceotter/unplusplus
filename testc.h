#include <stdint.h>

/* test.h - abbreviated from example */
typedef struct foo {
  int a, b;
  char c[3];

  struct {
    unsigned int b0 : 2, b1 : 3;

    struct {
      char x, y;
    } s;
  } x[2];
} foo_t;

foo_t* get_foo();
void free_foo(foo_t *foo);
int* get_int();
