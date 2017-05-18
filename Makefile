COMPILER_FLAGS := -O3 -std=c++1z -fno-exceptions -fno-stack-protector -fno-rtti -ffreestanding -fvisibility=hidden -Wall -Wsign-compare -I. -DGIT_REF=$(shell git rev-parse HEAD | tr '[:lower:]' '[:upper:]')
LINKER_FLAGS := #-Wl,-s
SOURCES := Foundation/* Storage/* Ontology/* External/*
PLATFORM = $(shell uname)

BUILD_PATH = build/
$(BUILD_PATH):
	mkdir -p $(BUILD_PATH)

# Build POSIX Executables
$(BUILD_PATH)SymatemMP: Targets/MP.cpp Targets/POSIX.hpp $(SOURCES) $(BUILD_PATH)
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $@ $<

$(BUILD_PATH)SymatemBp: Targets/Bp.cpp Targets/POSIX.hpp $(SOURCES) $(BUILD_PATH)
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $@ $<

$(BUILD_PATH)SymatemTests: Targets/Tests.cpp Targets/POSIX.hpp $(SOURCES) $(BUILD_PATH)
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $@ $<


# Run POSIX Executables
IMAGE_PATH = /dev/zero

runMP: $(BUILD_PATH)SymatemMP
	$< --path $(IMAGE_PATH)

runBp: $(BUILD_PATH)SymatemBp
	$< $(IMAGE_PATH)

runTests: $(BUILD_PATH)SymatemTests
	$< $(IMAGE_PATH)


# WebAssembly
WASM_TARGET = wasm32 # wasm64

$(BUILD_PATH)Symatem.s: Targets/WASM.cpp $(SOURCES) $(BUILD_PATH)
	$(LLVM_BIN)clang $(COMPILER_FLAGS) -target $(WASM_TARGET) -S -emit-llvm -o $(BUILD_PATH)Symatem.bc $<
	$(LLVM_BIN)llc -march=$(WASM_TARGET) -filetype=asm -o $(BUILD_PATH)Symatem.preAsm $(BUILD_PATH)Symatem.bc
	perl -pe 's/\.weak/# \.weak/g;' $(BUILD_PATH)Symatem.preAsm > $@
	rm $(BUILD_PATH)Symatem.bc $(BUILD_PATH)Symatem.preAsm

$(BUILD_PATH)Symatem.wast: $(BUILD_PATH)Symatem.s
	$(BINARYEN_BIN)s2wasm -o $@ $<

$(BUILD_PATH)Symatem.wasm: $(BUILD_PATH)Symatem.wast
	$(BINARYEN_BIN)wasm-as -o $@ $<



# Combined

buildAll: $(BUILD_PATH)SymatemMP $(BUILD_PATH)SymatemBp $(BUILD_PATH)SymatemTests $(BUILD_PATH)Symatem.wasm

clear:
	rm -Rf build/
