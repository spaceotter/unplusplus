#include <stdio.h>

#include "clib-test2.h"

#ifdef __cplusplus
#error "This is supposed to be compiled as C!"
#endif

int main(int argc, char *argv[]) {
  upp_geom_Point2D_int *a = upp_geom_Point2D_int_ctor(2, 3);
  upp_geom_Point2D_int *b = upp_geom_Point2D_int_ctor(5, 6);

  printf("Dot: %d\n", upp_geom_dot(a, b));

  upp_geom_Point2D_int_dtor(a);
  upp_geom_Point2D_int_dtor(b);
  return 0;
}
