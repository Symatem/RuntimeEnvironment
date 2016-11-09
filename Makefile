COMPILER_FLAGS := -O1 -std=c++1z -fno-exceptions -fno-stack-protector -fno-rtti -ffreestanding -fvisibility=hidden -Wall -Wsign-compare -I.
LINKER_FLAGS := #-Wl,-s
SOURCES := Storage/* Ontology/* HRL/* Targets/POSIX.hpp
PLATFORM = $(shell uname)


# Build POSIX Executables
POSIX_BUILD_PATH = build/

$(POSIX_BUILD_PATH):
	mkdir -p $(POSIX_BUILD_PATH)

$(POSIX_BUILD_PATH)SymatemBp: Targets/Bp.cpp $(SOURCES) $(POSIX_BUILD_PATH)
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $@ $<

$(POSIX_BUILD_PATH)SymatemAPI: Targets/API.cpp $(SOURCES) $(POSIX_BUILD_PATH)
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $@ $<

ifeq ($(PLATFORM), Linux)
 FUSE_PATH := /usr/include/fuse
 FUSE_NAME := fuse
endif
ifeq ($(PLATFORM), Darwin)
 FUSE_PATH := /usr/local/include/osxfuse
 FUSE_NAME := osxfuse
endif
$(POSIX_BUILD_PATH)SymatemFS: Targets/FS.cpp $(SOURCES) $(POSIX_BUILD_PATH)
	$(CC) $(COMPILER_FLAGS) -D_FILE_OFFSET_BITS=64 -I$(FUSE_PATH) $(LINKER_FLAGS) -l$(FUSE_NAME) -o $@ $<



# Test POSIX Executables
IMAGE_PATH = /dev/zero

runAPI: $(POSIX_BUILD_PATH)SymatemAPI
	$< --path $(IMAGE_PATH)

testBp: $(POSIX_BUILD_PATH)SymatemBp
	$< $(IMAGE_PATH)

MOUNT_PATH = $(POSIX_BUILD_PATH)mountpoint
testFS: $(POSIX_BUILD_PATH)SymatemFS
	mkdir -p $(MOUNT_PATH)
	$< $(IMAGE_PATH) $(MOUNT_PATH)



# WebAssembly
WASM_BUILD_PATH = ../WebAssembly/build/

$(WASM_BUILD_PATH):
	mkdir -p $(WASM_BUILD_PATH)

$(WASM_BUILD_PATH)Symatem.s: Targets/WASM.cpp $(SOURCES) $(WASM_BUILD_PATH)
	$(LLVM_BIN)clang $(COMPILER_FLAGS) -target wasm32 -c -emit-llvm -o $(WASM_BUILD_PATH)Symatem.bc $<
	$(LLVM_BIN)llc -march=wasm32 -filetype=asm -o $(WASM_BUILD_PATH)Symatem.preAsm $(WASM_BUILD_PATH)Symatem.bc
	perl -pe 's/\.weak/# \.weak/g;' $(WASM_BUILD_PATH)Symatem.preAsm > $@
	rm $(WASM_BUILD_PATH)Symatem.bc $(WASM_BUILD_PATH)Symatem.preAsm

$(WASM_BUILD_PATH)Symatem.wast: $(WASM_BUILD_PATH)Symatem.s
	$(BINARYEN_BIN)s2wasm -o $@ $<

$(WASM_BUILD_PATH)Symatem.wasm: $(WASM_BUILD_PATH)Symatem.wast
	$(BINARYEN_BIN)wasm-as -o $@ $<



# Combined

buildAll: $(POSIX_BUILD_PATH)SymatemBp $(POSIX_BUILD_PATH)SymatemAPI $(POSIX_BUILD_PATH)SymatemFS $(WASM_BUILD_PATH)Symatem.wasm

testAll: testBp testFS

clear:
	rm -Rf build/
