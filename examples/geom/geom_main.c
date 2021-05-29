#include <stdio.h>

#include "geom-clib.h"

#ifdef __cplusplus
#error "This is supposed to be compiled as C!"
#endif

int main(int argc, char *argv[]) {
  upp_geom_Point2D_int *a = upp_new_geom_Point2D_int_3(2, 3);
  struct upp_geom_Point2D_int_s_ b = {{5, 6}};

  printf("Dot: %d\n", upp_geom_dot(a, &b));

  upp_del_geom_Point2D_int(a);

  upp_std_vector_geom_Point_double_3_std_allocator_geom_Point_double_3 *v =
      upp_new_std_vector_geom_Point_double_3_std_allocator_geom_Point_double_3();
  upp_geom_Point_double_3 *pt = NULL;
  pt = upp_new_geom_Point_double_3_2();
  upp_geom_Point_double_3_set_axes(pt, 0, 1);
  upp_geom_Point_double_3_set_axes(pt, 1, 2);
  upp_geom_Point_double_3_set_axes(pt, 2, 3);
  upp_std_vector_geom_Point_double_3_std_allocator_geom_Point_double_3_push_back(v, pt);
  pt = upp_new_geom_Point_double_3_2();
  upp_geom_Point_double_3_set_axes(pt, 0, 4);
  upp_geom_Point_double_3_set_axes(pt, 1, 5);
  upp_geom_Point_double_3_set_axes(pt, 2, 6);
  upp_std_vector_geom_Point_double_3_std_allocator_geom_Point_double_3_push_back(v, pt);
  pt = upp_new_geom_Point_double_3_2();
  upp_geom_centroid_double_3(pt, v);
  printf("Centroid: %f, %f, %f\n",
         upp_geom_Point_double_3_get_axis(pt, 0),
         upp_geom_Point_double_3_get_axis(pt, 1),
         upp_geom_Point_double_3_get_axis(pt, 2));
  upp_del_geom_Point_double_3(pt);
  upp_del_std_vector_geom_Point_double_3_std_allocator_geom_Point_double_3(v);
  return 0;
}
