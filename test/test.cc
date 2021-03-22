#include "test.hh"

using namespace c;

Point::Point(int x, int y) : x(x), y(y) {}

int Point::magSQ() {
  return x*x + y*y;
}

Point Point::add(Point other) {
  return {other.x + x, other.y + y};
}

Point *Point::addTo(Point *other) {
  other->x += x;
  other->y += y;
  return other;
}
