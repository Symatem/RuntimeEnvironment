[![Build Status](https://secure.travis-ci.org/Symatem/CppCodeBase.svg)](http://travis-ci.org/Symatem/CppCodeBase)

RTE in C++17
============

Symatem RTE C++ code base as opposed to the native code base which will be based on this one later on.


Targets
-------

### B+ Tree Test
To test the stability of the ontology engine.

### FUSE
The ontology engine can be used to mount a FUSE by
emulating a POSIX hierarchical FS and use it for stability testing.

### MessagePack API
TCP socket based API of the ontology engine.

### WebAssembly API
Ontology engine running inside a browser or node.js.

### Unikernel
Combined with [UnikernelExperiments](https://github.com/Lichtso/UnikernelExperiments) this will be an intermediate platform to test the entrie RTE functionallity in the future. Later on we might switch to [RISC-V](https://riscv.org) as an ISA for running on the hardware directly without any additional layers.


Coding Restrictions
-------------------

- No goto, dynamic_cast
- No new/delete, malloc/free => heapless
- No recursion/alloca => fixed stack size
- No StdLibC++ => core is zero dependency
- No exceptions
