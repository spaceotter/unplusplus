namespace A {
typedef int my_int;
}
namespace B {
class foo;
}
namespace C {
typedef B::foo my_foo;
}
namespace D {
typedef C::my_foo* foo_ptr;
}
namespace E {
typedef C::my_foo& foo_ref;
}
typedef int int_array[5];
typedef int (*op_ptr)(int a, int b);
