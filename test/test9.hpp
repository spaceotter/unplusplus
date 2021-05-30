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
    } a;
    struct {
      char c;
      char d;
    };
  };
};

struct test2a {
  int T;
};

struct test2 : public virtual test2a {
  union {
    struct {
      int S;
      int b;
    } S;
    struct {
      char c;
      char d;
    } T;
  } vtable;
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
