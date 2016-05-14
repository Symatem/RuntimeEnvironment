CPPOPTIONS := -std=c++1z -fno-exceptions -fno-stack-protector -fno-use-cxa-atexit -fvisibility=hidden -Wall
SOURCES := Storage/* Ontology/* Interpreter/* Targets/POSIX.hpp
STDPATH := ../StandardLibrary

test: build/ build/SymatemHRL
	build/SymatemHRL ./data $(STDPATH)/Foundation/ -e $(STDPATH)/Tests/

clear:
	rm -f build/*

build/:
	mkdir -p build

build/BpTest: $(SOURCES) Targets/BpTest.cpp
	$(CC) $(CPPOPTIONS) -o $@ Targets/BpTest.cpp

build/SymatemHRL: $(SOURCES) Targets/HRL.cpp
	$(CC) $(CPPOPTIONS) -o $@ Targets/HRL.cpp

build/WASM.bc: $(SOURCES) Targets/WASM.cpp
	$(LLVM_BIN)/clang $(CPPOPTIONS) -O3 -target wasm32 -c -emit-llvm -o $@ Targets/WASM.cpp

build/WASM.asm: build/WASM.bc
	$(LLVM_BIN)/llc -march=wasm32 -filetype=asm -o build/WASM.pre_asm build/WASM.bc
	perl -pe 's/\.weak/# \.weak/g;' build/WASM.pre_asm > $@
	rm build/WASM.pre_asm

build/Symatem.wast: build/WASM.asm
	$(BINARYEN_BIN)/s2wasm build/WASM.asm > $@

build/Symatem.wasm: build/Symatem.wast
	$(PROTO_BIN)/sexpr-wasm -o $@ build/Symatem.wast
