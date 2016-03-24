[![Build Status](https://secure.travis-ci.org/Symatem/CppCodeBase.svg)](http://travis-ci.org/Symatem/CppCodeBase)

RTE in C++11
============

Symatem RTE C++ code base as opposed to the native code base which will be based on this one later on.

Coding Restrictions:
- No goto
- No virtual/polymorphism/dynamic_cast
- No new/delete, malloc/free => heapless (WIP)
- No recursion/alloca, fixed stack size
- No StdLibC++ => almost zero dependency (except from LibC)
- No exceptions
