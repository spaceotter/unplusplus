namespace A {
template <class T>
struct Foo {
  T m;
  T get() { return m; }
};
}  // namespace A
namespace B {
typedef A::Foo<int> Bar;
typedef A::Foo<float> Barf;
}  // namespace B

extern template class A::Foo<double>;
