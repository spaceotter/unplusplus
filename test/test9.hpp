struct test {
  struct innertest {
    int a;
    int b;
  };
  innertest i;
  union {
    struct {
      int a;
      int b;
    };
    struct {
      char c;
      char d;
    };
  };
};

struct test2 {
  union {
    struct {
      int a;
      int b;
    } S;
    struct {
      char c;
      char d;
    } T;
  } U;
};

typedef struct {
  int a;
  int b;
} bab_t;

struct test3 {
  bab_t x;
  bab_t y;
};

struct test4 {
  template <class T>
  struct test5 {
    T a;
    T b;
  };
  test5<int> m;
};
