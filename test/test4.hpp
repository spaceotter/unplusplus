namespace A {
struct Too {
  long loo;
};
struct Foo : public Too {
  long cat;
  Foo(long c) : cat(c) {}
  ~Foo() {};
  virtual int f(Foo b) const { return b.cat + cat; };
};
struct Bar : public virtual Foo {
  Bar() : Foo(1L) {}
  long birb;
  virtual int g(const Foo b) { return b.cat + birb; };
  int f(Foo b) const override { return b.cat + birb; };
};
struct Baz : public virtual Foo {
  Baz() : Foo(2L) {}
  long rat;
};
struct Oof : public Bar, public Baz {
  Oof() : Foo(3L) {}
  long tar;
};
struct Moo : public Too {
  long cow;
};
struct Boo : public Oof, public Moo {
  Boo() : Foo(4L) {}
  long goo;
};
extern Bar bar;
}  // namespace A
