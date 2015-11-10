INCLUDES :=
CPPOPTIONS := -std=c++1y -stdlib=libc++ ${PGO_OPTIONS} ${INCLUDES}
BUILDDIR := build
STDPATH := ../StandardLibrary

# PGO_OPTIONS := -fprofile-instr-generate
# CPPOPTIONS := -O3 -flto
# -fprofile-sample-use=code.prof



run: $(BUILDDIR)/CLI
	$(BUILDDIR)/CLI $(STDPATH)/stdlib/

test: $(BUILDDIR)/CLI
	$(BUILDDIR)/CLI $(STDPATH)/stdlib/ -e $(STDPATH)/tests/

$(BUILDDIR)/CLI: $(BUILDDIR)/
	clang++ ${CPPOPTIONS} -o $(BUILDDIR)/CLI CLI/main.cpp

$(BUILDDIR)/:
	mkdir -p build
