#include <stdio.h>
#include "libtest.h"

#ifdef __cplusplus
#error "This is supposed to be compiled as C!"
#endif

int main(int argc, char *argv[])
{
  upp_c_Point *a = upp_new_upp_c_Point(2, 3);
  upp_c_Point *b = upp_new_upp_c_Point(4, 5);

  upp_c_Point_addTo(a, b);
  upp_c_Point_print(a);
  upp_c_Point_print(b);

  upp_del_upp_c_Point(a);
  upp_del_upp_c_Point(b);
  return 0;
}
