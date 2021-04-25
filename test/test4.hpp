namespace A {
struct Foo {
  Foo(long c);
  ~Foo();
  int f(Foo b);
};
struct Bar {
  int g(Foo b);
};
}
