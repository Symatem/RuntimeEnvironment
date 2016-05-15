CPPOPTIONS := -std=c++1z -fno-exceptions -fno-stack-protector -fno-use-cxa-atexit -fvisibility=hidden -Wall
SOURCES := Storage/* Ontology/* Interpreter/* Targets/POSIX.hpp
STDPATH := ../StandardLibrary

test: build/ build/SymatemHRL
	build/SymatemHRL ./data $(STDPATH)/Foundation/ -e $(STDPATH)/Tests/

clear:
	rm -f build/*

build/:
	mkdir -p build

build/BpTest: Targets/BpTest.cpp $(SOURCES)
	$(CC) $(CPPOPTIONS) -o $@ $<

build/SymatemHRL: Targets/HRL.cpp $(SOURCES)
	$(CC) $(CPPOPTIONS) -o $@ $<

build/WASM.asm: Targets/WASM.cpp $(SOURCES)
	$(LLVM_BIN)/clang $(CPPOPTIONS) -O3 -target wasm32 -c -emit-llvm -o build/WASM.bc $<
	$(LLVM_BIN)/llc -march=wasm32 -filetype=asm -o build/WASM.pre_asm build/WASM.bc
	perl -pe 's/\.weak/# \.weak/g;' build/WASM.pre_asm > $@
	rm build/WASM.bc build/WASM.pre_asm

build/Symatem.wast: build/WASM.asm
	$(BINARYEN_BIN)/s2wasm $< > $@

build/Symatem.wasm: build/Symatem.wast
	$(PROTO_BIN)/sexpr-wasm -o $@ $<

all: build/BpTest build/SymatemHRL build/SymatemFS build/Symatem.wasm
