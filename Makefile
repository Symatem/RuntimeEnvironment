CPPOPTIONS := -std=c++1y -fno-exceptions
STDPATH := ../StandardLibrary
BUILDDIR := build
TARGET_POSIX := $(BUILDDIR)/SymatemRTE

$(TARGET_POSIX):
	mkdir -p $(BUILDDIR)
	$(CC) $(CPPOPTIONS) -o $(TARGET_POSIX) main.cpp

test: $(TARGET_POSIX)
	$(TARGET_POSIX) $(STDPATH)/Foundation/ -e $(STDPATH)/Tests/

clear:
	rm -fr $(BUILDDIR)

rebuild: clear $(TARGET_POSIX)
