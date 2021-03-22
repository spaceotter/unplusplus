#ifndef TEST_CIF_H
#define TEST_CIF_H

#ifdef __cplusplus
#include "test.hh"
typedef c::Point c_Point;
#else
typedef struct _struct_c_Point c_Point;
#endif

#ifdef __cplusplus
extern "C" {
#endif

c_Point *new_c_Point();
void del_c_Point(c_Point *_cif_this);
int c_Point_magSQ(c_Point *_cif_this);

#ifdef __cplusplus
}
#endif

#endif /* TEST_CIF_H */
