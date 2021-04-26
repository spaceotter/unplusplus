namespace A {
namespace B {
struct grue {};
}  // namespace B
}  // namespace A
namespace C {
namespace D {
void foo();
int bar(int a, int b);
float bar(float a, float b);
float bar_2(float a, float b);
A::B::grue baz(A::B::grue x);
A::B::grue* baz(A::B::grue* x);
A::B::grue& bazr(A::B::grue& x);
}  // namespace D
}  // namespace C
int (*foo(A::B::grue a))(A::B::grue a, int b);
