namespace A {
int foo(int a);
extern "C" int bar(int b);
extern "C" int varg(int a, ...);
int warg(int a, ...);
extern "C" {
float bab(float b);
}
}  // namespace A
