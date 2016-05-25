CPPOPTIONS := -std=c++1z -fno-exceptions -fno-stack-protector -fno-use-cxa-atexit -fvisibility=hidden -Wall
SOURCES := Storage/* Ontology/* Interpreter/* Targets/POSIX.hpp
STD_PATH := ../StandardLibrary
IMAGE_PATH := /dev/zero

buildAll: build/SymatemBp build/SymatemHRL build/SymatemFS build/Symatem.wasm

testAll: testBp testHRL testFS

clear:
	rm -Rf build/

build/:
	mkdir -p build

build/SymatemBp: Targets/Bp.cpp $(SOURCES) build/
	$(CC) $(CPPOPTIONS) -o $@ $<

build/SymatemHRL: Targets/HRL.cpp $(SOURCES) build/
	$(CC) $(CPPOPTIONS) -o $@ $<

build/SymatemFS: Targets/FS.cpp $(SOURCES) build/
	$(CC) $(CPPOPTIONS) -I$(FUSE_LIB)/include -L/usr/local/lib -losxfuse -D_FILE_OFFSET_BITS=64 -o $@ $<

build/WASM.asm: Targets/WASM.cpp $(SOURCES) build/
	$(LLVM_BIN)/clang $(CPPOPTIONS) -O3 -target wasm32 -c -emit-llvm -o build/WASM.bc $<
	$(LLVM_BIN)/llc -march=wasm32 -filetype=asm -o build/WASM.pre_asm build/WASM.bc
	perl -pe 's/\.weak/# \.weak/g;' build/WASM.pre_asm > $@
	rm build/WASM.bc build/WASM.pre_asm

build/Symatem.wast: build/WASM.asm
	$(BINARYEN_BIN)/s2wasm $< > $@

build/Symatem.wasm: build/Symatem.wast
	$(PROTO_BIN)/sexpr-wasm -o $@ $<

testBp: build/SymatemBp
	$< $(IMAGE_PATH)

testHRL: build/SymatemHRL
	$< $(IMAGE_PATH) $(STD_PATH)/Foundation/ -e $(STD_PATH)/Tests/

testFS: build/SymatemFS
	mkdir -p build/mountpoint
	$< $(IMAGE_PATH) build/mountpoint
