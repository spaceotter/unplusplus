#include "test.hh"

using namespace c;

Point::Point(int x, int y) : x(x), y(y) {}

int Point::magSQ() {
  return x*x + y*y;
}
