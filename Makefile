CPPOPTIONS := -std=c++1z -fno-exceptions -fno-stack-protector -fno-use-cxa-atexit -fvisibility=hidden -Wall
SOURCES := Storage/* Ontology/* Interpreter/*
STDPATH := ../StandardLibrary

test: build/ build/SymatemHRL
	build/SymatemHRL ./data $(STDPATH)/Foundation/ -e $(STDPATH)/Tests/

clear:
	rm -f build/*

build/:
	mkdir -p build

build/BpTest: $(SOURCES) Targets/POSIX.hpp Targets/BpTest.cpp
	$(CC) $(CPPOPTIONS) -o $@ Targets/BpTest.cpp

build/SymatemHRL: $(SOURCES) Targets/POSIX.hpp Targets/HRL.cpp
	$(CC) $(CPPOPTIONS) -o $@ Targets/HRL.cpp

build/WASM.llvm: $(SOURCES) Targets/WASM.cpp
	$(LLVM_BIN)/clang-3.9 $(CPPOPTIONS) -DWEB_ASSEMBLY -O3 -S -emit-llvm -o $@ Targets/WASM.cpp

build/WASM.asm: build/WASM.llvm
	$(LLVM_BIN)/llc -march=wasm32 -filetype=asm -o build/pre.asm build/WASM.llvm
	perl -pe 's/\.weak/# \.weak/g;' build/pre.asm > $@
	rm build/pre.asm

build/Symatem.wast: build/WASM.asm
	$(BINARYEN_BIN)/s2wasm build/WASM.asm > $@

build/Symatem.wasm: build/Symatem.wast
	$(PROTO_BIN)/sexpr-wasm -o $@ build/Symatem.wast
