# unplusplus

This project aims to generate from a C++ header a C++ library containing stub with C linkage that
allow basic operation of C++ from C. I only just started so it doesn't yet. Check back later.

Theoretical workflow:
1. Create a header with all the desired parts of the target library included
2. Run it through c2ffi to get a spec file for the target library
3. Run unplusplus on the spec file to get a header&source file with C stubs
4. Build the stubs as a separate library if the target is a system library, or build them into the
   target library.
5. Use the unplusplus header as a foreign function interface.
