INCLUDES :=
CPPOPTIONS := -std=c++1y -stdlib=libc++ ${PGO_OPTIONS} ${INCLUDES}
BUILDDIR := build
TARGET := $(BUILDDIR)/CLI
STDPATH := ../StandardLibrary

# PGO_OPTIONS := -fprofile-instr-generate
# CPPOPTIONS := -O3 -flto
# -fprofile-sample-use=code.prof

$(TARGET):
	mkdir -p $(BUILDDIR)
	clang++ ${CPPOPTIONS} -o $(TARGET) CLI/main.cpp

run: $(TARGET)
	$(TARGET) $(STDPATH)/StdLibrary/

test: $(TARGET)
	$(TARGET) $(STDPATH)/StdLibrary/ -e $(STDPATH)/StdTests/

clear:
	rm -fr $(BUILDDIR)

rebuild: clear $(TARGET)
