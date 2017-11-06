[![Build Status](https://secure.travis-ci.org/Symatem/RuntimeEnvironment.svg)](http://travis-ci.org/Symatem/RuntimeEnvironment)

# Symatem Runtime Environment
Currently written in C++17.
It will be replaced by its own programming model.


## Targets

### API (Virtual Machine) based on WebAssembly
You will need to clone and compile [Wasm Binaryen](https://github.com/WebAssembly/binaryen) for this target.

`BINARYEN_BIN=binaryen/bin/ make build/Symatem.wasm`

### API (POSIX Process) based on MessagePack over TCP
`make runMP`

### Tests (POSIX Process)
`make runTests`

### Unikernel
Combined with [UnikernelExperiments](https://github.com/Lichtso/UnikernelExperiments) this will be an intermediate platform to test the entire RTE functionality in the future. Later on we might switch to [RISC-V](https://riscv.org) as an ISA for running on the hardware directly without any additional layers.


## Coding Restrictions
- No goto, dynamic_cast and virtual methods
- No new/delete, malloc/free => custom memory allocator
- No recursion/alloca => fixed stack bounds
- No StdLibC++ => core is zero dependency
- No exceptions
