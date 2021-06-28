# unplusplus

This project aims to automatically generate a C interface to C++. More precisely, it generates
source code for a C++ library from a C++ header, that contains stubs with C linkage that allow basic
usage of C++ features from C, as described below. The expected use case is to allow a C-only foreign
function interface (FFI) to use a C++ library.

## Workflow:
1. Build the target C++ library.
2. Create a header with all the desired parts of the target library included.
3. Run unplusplus on that header to generate the C++ to C interface, along with all the compiler options that were used on the library.
4. If you encounter problems, see the Limitations section.
5. Build the generated stubs with the C++ compiler.
6. Build the C code that uses the generated header with the C compiler.
7. Link the C++ stub library and the C object file with the C++ linker.

It is recommended to use the clang to compile the project, whose libclang unplusplus was built with,
to avoid issues with system header or ABI incompatibility.

## Generated Code

* If the C++ code includes headers that are known to be C system headers, all the symbols are
  skipped and that header is included.
* `extern "C"` symbols will be declared, but not defined.
* Namespaces and templates are collapsed according to a name-mangling system. For instance,
  `A::foo<int>` becomes `upp_A_foo_int`
* It will try to fully instantiate template specializations that were hinted at in the supplied
  code, since templates normally use lazy evaluation.
* Redundant code will not be emitted for `using` declarations.
* Constructors will generate a function that allocates a new object.
* Destructors will generate a function that frees an object.
* Class methods will generate a function that invokes the method on an object.
* Overridden functions will be given a unique name by appending a number.
* Any passing of structs/classes by value or C++ `&` reference are changed to passing by pointer,
  for the best compatibility with FFI.
* Returning structs/classes by value is changed to supplying a pointer argument for the output to be
  assigned into.
* Functions declarations with arguments without a provided name are given generated ones.
* Enumeration values are copied to the output.
* Enumerations with a non-int type are generated as a bunch of macros.
* A constant extern *pointer* is emitted for global variables, which points to the variable.
* A struct that mirrors the layout of the C++ type is emitted to allow direct access to
  fields. Anonymous unions and structs used to organize fields are copied directly over to the C
  struct.

## Filtering Declarations

If you need to leave out some declarations, you can supply its fully qualified name with the `-e`
option, or add the fully qualified name as a line in a text file and supply the file with the
`--excludes-file` option. Lines that are empty or start with `#` are ignored.

Declarations that are excluded in this way should not appear in the generated code, and any
declarations using them, such as a function with an excluded type as a parameter, are also omitted.

If an excluded declaration is renamed with a typedef, that is not itself also excluded, then the
type will be referred to by later declarations using the typedef name instead.

Some declarations are always filtered out in this way. These are implementation-specific parts that
are internal to the standard C++ library, such as the true name of iterators. The container
template's typedefs for the iterator get used instead.

You can also filter out "deprecated" declarations (created with `__attribute__((deprecated))` for
instance) with the option `--no-deprecated`.

## Limitations

The project is not ready for general use yet.

* Some newer C++ features may not be supported.
* Macros are not transferred.
* The generator does not account for explicit alignment or packed structure declarations. If these
  are present, the layout of mirror structs could be incorrect.
* Templates are normally evaluated in a lazy fashion. This means that methods are not instantiated
  unless they are used, which can result in hidden bugs in libraries where the template's method is
  incompatible with some template arguments. These can be surfaced by unplusplus because it
  explicitly uses every single member of every template specialization that it can find. To resolve
  this: create an excludes file that lists the broken template methods and pass it to unplusplus.
* unplusplus tries to emit include directives for C standard library headers, and omit the
  declarations in them for brevity. Unfortunately, there doesn't seem to be a reliable way to tell
  *which* headers are the C standard library other than listing them all tediously.
* unplusplus can't handle [variadic functions](https://en.cppreference.com/w/cpp/utility/variadic)
  with C++ linkage, since it [isn't possible](http://c-faq.com/varargs/handoff.html) to forward the
  va_list to a variadic function.

## Development

When developing on the project, it is recommended to link it to a different build of clang and LLVM
that has the debugging symbols and assertions enabled. The clang assertions catch many errors that
would otherwise be very hard to debug.
