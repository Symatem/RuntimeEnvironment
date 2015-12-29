CPPOPTIONS := -std=c++1y -stdlib=libc++ ${PGO_OPTIONS}
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

installLinuxDependencies:
	svn co http://llvm.org/svn/llvm-project/libcxx/trunk libcxx
	mkdir build_libcxx
	cd build_libcxx
	CC=clang CXX=clang++ cmake -G "Unix Makefiles" -DLIBCXX_CXX_ABI=libsupc++ -DLIBCXX_LIBSUPCXX_INCLUDE_PATHS="/usr/include/c++/4.6/;/usr/include/c++/4.6/x86_64-linux-gnu/" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr $HOME/Clang/libcxx
	make
	sudo make install

clear:
	rm -fr $(BUILDDIR)

rebuild: clear $(TARGET)
