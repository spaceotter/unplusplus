#include "test-cif.h"

c_Point *new_c_Point(int x, int y) {
  return new c::Point(x, y);
}
void del_c_Point(c_Point *_cif_this) {
  delete _cif_this;
}

int c_Point_magSQ(c_Point *_cif_this) {
  return _cif_this->magSQ();
}
