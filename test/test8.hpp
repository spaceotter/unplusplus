namespace A {
enum { A, B };
enum : char { moo, mooo };
enum B {
  boo, booo
};
typedef enum { C, D } letter_t;
typedef enum { sC = -1, sD } sletter_t;
enum class Z : unsigned char { E, F };
typedef struct {
  int x;
  int y;
} point_t;
}  // namespace A
void foo(A::Z a);
void bar(enum A::B b);
