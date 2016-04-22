CPPOPTIONS := -std=c++1z -fno-exceptions -fno-stack-protector -fno-use-cxa-atexit -fvisibility=hidden
STDPATH := ../StandardLibrary
BUILDDIR := build
TARGET_NAME := SymatemRTE

posix: $(BUILDDIR)/ $(BUILDDIR)/$(TARGET_NAME)

wasm: $(BUILDDIR)/ $(BUILDDIR)/$(TARGET_NAME).wasm

all: posix wasm

test: posix
	$(BUILDDIR)/$(TARGET_NAME) $(STDPATH)/Foundation/ -e $(STDPATH)/Tests/

clear:
	rm -f $(BUILDDIR)/*

rebuild: clear all

$(BUILDDIR)/:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/$(TARGET_NAME): Platform/POSIX.cpp
	$(CC) $(CPPOPTIONS) -o $(BUILDDIR)/$(TARGET_NAME) Platform/POSIX.cpp

$(BUILDDIR)/$(TARGET_NAME).llvm: Platform/WASM.cpp
	$(LLVM_BIN)/clang-3.9 $(CPPOPTIONS) -DWEB_ASSEMBLY -O3 -S -emit-llvm -o $(BUILDDIR)/$(TARGET_NAME).llvm Platform/WASM.cpp

$(BUILDDIR)/$(TARGET_NAME).asm: $(BUILDDIR)/$(TARGET_NAME).llvm
	$(LLVM_BIN)/llc -march=wasm32 -filetype=asm -o $(BUILDDIR)/pre.asm $(BUILDDIR)/$(TARGET_NAME).llvm
	perl -pe 's/\.weak/# \.weak/g;' $(BUILDDIR)/pre.asm > $(BUILDDIR)/$(TARGET_NAME).asm
	rm $(BUILDDIR)/pre.asm

$(BUILDDIR)/$(TARGET_NAME).wast: $(BUILDDIR)/$(TARGET_NAME).asm
	$(BINARYEN_BIN)/s2wasm $(BUILDDIR)/$(TARGET_NAME).asm > $(BUILDDIR)/$(TARGET_NAME).wast

$(BUILDDIR)/$(TARGET_NAME).wasm: $(BUILDDIR)/$(TARGET_NAME).wast
	$(PROTO_BIN)/sexpr-wasm -o $(BUILDDIR)/$(TARGET_NAME).wasm $(BUILDDIR)/$(TARGET_NAME).wast
