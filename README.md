[![Build Status](https://secure.travis-ci.org/Symatem/VirtualMachine.png)](http://travis-ci.org/Symatem/VirtualMachine)

RTE in C++11
============

C++ code base of the Symatem RTE.
This will be used for the WebAssembly, RumpRun unikernel and RISC-V bare metal ports.

Coding Restrictions:
- No goto
- No virtual/polymorphism/dynamic_cast
- No new/delete, malloc/free => heapless (WIP)
- No recursion/alloca, fixed stack size
- No LibStdC++ => almost zero dependency (except from LibC)
- No exceptions
