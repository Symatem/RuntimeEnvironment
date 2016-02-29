CPPOPTIONS := -std=c++1y -stdlib=libc++ -fno-exceptions ${PGO_OPTIONS}
BUILDDIR := build
TARGET := $(BUILDDIR)/CLI
STDPATH := ../StandardLibrary

# PGO_OPTIONS := -fprofile-instr-generate
# CPPOPTIONS := -O3 -flto
# -fprofile-sample-use=code.prof

$(TARGET):
	mkdir -p $(BUILDDIR)
	$(CXX) ${CPPOPTIONS} -o $(TARGET) CLI/main.cpp

run: $(TARGET)
	$(TARGET) $(STDPATH)/Foundation/

test: $(TARGET)
	$(TARGET) -t $(STDPATH)/Foundation/ -e $(STDPATH)/Tests/

clear:
	rm -fr $(BUILDDIR)

rebuild: clear $(TARGET)
