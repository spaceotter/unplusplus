#pragma once

#include <stdlib.h>

namespace c {
  struct Point {
    int x;
    int y;
    Point(int x, int y);
    int magSQ();
    Point add(Point other);
    Point *addTo(Point *other);
    void print();
  };
}
