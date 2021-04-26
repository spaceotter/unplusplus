template <class T>
T add(T a, T b) {
  return a + b;
}

extern template int add<int>(int, int);
