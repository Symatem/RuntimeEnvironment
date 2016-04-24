[![Build Status](https://secure.travis-ci.org/Symatem/CppCodeBase.svg)](http://travis-ci.org/Symatem/CppCodeBase)

RTE in C++11
============

Symatem RTE C++ code base as opposed to the native code base which will be based on this one later on.

Coding Restrictions:
- No goto, dynamic_cast
- No new/delete, malloc/free => heapless
- No recursion/alloca => fixed stack size
- No StdLibC++ => core is zero dependency
- No exceptions
