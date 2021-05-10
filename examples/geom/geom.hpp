#pragma once

#include <vector>

namespace geom {
template <class T, int N>
class Point {
 protected:
  T vals[N];

 public:
  T get_axis(int n) const { return vals[n]; }
  void set_axes(int n, T v) { vals[n] = v; }
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
typedef Point2D<double> Point2Dd;
int dot(Point2Di a, Point2Di b);
float dot(Point2D<float> &a, Point2D<float> &b);
double dot_2(Point2D<double> *a, Point2D<double> *b);

template <class T, int N>
Point<T, N> centroid(const std::vector<Point<T, N>> &points) {
  Point<T, N> r;
  for (int n = 0; n < N; n++) {
    T mean = (T)0;
    for (const auto &p : points) {
      mean += p.get_axis(n);
    }
    mean /= (T)points.size();
    r.set_axes(n, mean);
  }
  return r;
}
extern template Point<double, 3> centroid<double, 3>(const std::vector<Point<double, 3>> &points);
}  // namespace geom
