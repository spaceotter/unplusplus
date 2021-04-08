#pragma once

namespace c {
  struct Point {
    int x;
    int y;
    Point(int x, int y);
    ~Point() {};
    int magSQ();
    Point add(const Point &other) const;
    Point *addTo(Point *other);
    void print();
    Point operator+(const Point &other) const;
  };

  template <class T, int L>
  struct Size {
    T W;
    T H;
    T a[L];
  };
}

typedef c::Size<int, 5> iSize;
