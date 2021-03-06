#include "geom.hpp"

#include <iostream>

using namespace geom;

template class geom::Point2D<int>;
template Point<double, 3> geom::centroid<double, 3>(const std::vector<Point<double, 3>> &points);

int geom::dot(Point2Di a, Point2Di b) { return a.x() * b.x() + a.y() * b.y(); }

float geom::dot(Point2D<float> &a, Point2D<float> &b) { return a.x() * b.x() + a.y() * b.y(); }

double geom::dot_2(Point2D<double> *a, Point2D<double> *b) {
  return a->x() * b->x() + a->y() * b->y();
}
