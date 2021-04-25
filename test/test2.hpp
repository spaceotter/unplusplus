#pragma once

#include <math.h>
#include <stdint.h>

extern uint8_t blah;

namespace geom {
template <class T, int N>
class Point {
 protected:
  T vals[N];

 public:
  T get_axis(int n) {
    if (n < N) return vals[n];
  }
  Point<T, N> operator+(const Point &other) {
    Point<T, N> ret;
    for (int i = 0; i < N; i++) {
      ret.vals[i] = vals[i] + other.vals[i];
    }
    return ret;
  }
};

template <class T>
class Point2D : Point<T, 2> {
 public:
  Point2D(T x, T y) {
    Point<T, 2>::vals[0] = x;
    Point<T, 2>::vals[1] = y;
  }
  T x() { return Point<T, 2>::vals[0]; }
  T y() { return Point<T, 2>::vals[1]; }
};

typedef Point2D<int> Point2Di;
int dot(Point2Di a, Point2Di b);
float dot(Point2D<float> &a, Point2D<float> &b);
double dot_2(Point2D<double> *a, Point2D<double> *b);
}  // namespace geom
