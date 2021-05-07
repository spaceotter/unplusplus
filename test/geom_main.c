#include <stdio.h>

#include "clib-geom.h"

#ifdef __cplusplus
#error "This is supposed to be compiled as C!"
#endif

int main(int argc, char *argv[]) {
  upp_geom_Point2D_int *a = upp_new_geom_Point2D_int(2, 3);
  struct upp_geom_Point2D_int_s_ b = {{5, 6}};

  printf("Dot: %d\n", upp_geom_dot(a, &b));

  upp_del_geom_Point2D_int(a);
  return 0;
}
