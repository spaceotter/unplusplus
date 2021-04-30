namespace A {
struct Foo {
  Foo(long c);
  ~Foo();
  int f(Foo b) const;
};
struct Bar {
  int g(const Foo b);
};
}  // namespace A
