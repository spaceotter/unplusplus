#include <iostream>

#include "test-clib.h"

A::Bar A::bar;

int main(int argc, char *argv[]) {
  struct upp_A_Boo_s_ upp_A_Boo_test;
  A::Boo upp_A_Boo_test_cxx;
  std::cout << "C rat " << ((char *)&(upp_A_Boo_test.rat) - (char *)&upp_A_Boo_test) << std::endl;
  std::cout << "C++ rat " << ((char *)&(upp_A_Boo_test_cxx.rat) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;
  std::cout << "C cat " << ((char *)&(upp_A_Boo_test.cat) - (char *)&upp_A_Boo_test) << std::endl;
  std::cout << "C++ cat " << ((char *)&(upp_A_Boo_test_cxx.cat) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;
  std::cout << "C tar " << ((char *)&(upp_A_Boo_test.tar) - (char *)&upp_A_Boo_test) << std::endl;
  std::cout << "C++ tar " << ((char *)&(upp_A_Boo_test_cxx.tar) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;
  std::cout << "C birb " << ((char *)&(upp_A_Boo_test.birb) - (char *)&upp_A_Boo_test) << std::endl;
  std::cout << "C++ birb " << ((char *)&(upp_A_Boo_test_cxx.birb) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;
  std::cout << "C goo " << ((char *)&(upp_A_Boo_test.goo) - (char *)&upp_A_Boo_test) << std::endl;
  std::cout << "C++ goo " << ((char *)&(upp_A_Boo_test_cxx.goo) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;
  std::cout << "C cow " << ((char *)&(upp_A_Boo_test.cow) - (char *)&upp_A_Boo_test) << std::endl;
  std::cout << "C++ cow " << ((char *)&(upp_A_Boo_test_cxx.cow) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;

  std::cout << "C loo " << ((char *)&(upp_A_Boo_test.Boo_Moo_Too_loo) - (char *)&upp_A_Boo_test)
            << std::endl;
  std::cout << "C++ loo "
            << ((char *)&(((A::Moo *)&upp_A_Boo_test_cxx)->loo) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;

  std::cout << "C loo2 "
            << ((char *)&(upp_A_Boo_test.Boo_Oof_Bar_Foo_Too_loo) - (char *)&upp_A_Boo_test)
            << std::endl;
  std::cout << "C++ loo2 "
            << ((char *)&(((A::Oof *)&upp_A_Boo_test_cxx)->loo) - (char *)&upp_A_Boo_test_cxx)
            << std::endl;

  return 0;
}
