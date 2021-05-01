namespace A {
enum { A, B };
typedef enum { C, D } letter_t;
enum class Z : unsigned char { E, F };
typedef struct {
  int x;
  int y;
} point_t;
}  // namespace A
void foo(A::Z a);
