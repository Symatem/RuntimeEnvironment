#include "../Interpreter/Procedures.hpp"

extern "C" {
    unsigned memcpy(unsigned dst, unsigned src, unsigned len) {
        for(unsigned i = 0; i < len; ++i)
            *reinterpret_cast<char*>(dst+i) = *reinterpret_cast<char*>(src+i);
        return 0;
    }

    unsigned memset(unsigned dst, unsigned value, unsigned len) {
        for(unsigned i = 0; i < len; ++i)
            *reinterpret_cast<char*>(dst+i) = value;
        return 0;
    }
};

__attribute__((noinline)) unsigned getMemorySize() {
    unsigned result;
    asm("memory_size $0=");
    return result;
}

__attribute__((noinline)) void growMemory(unsigned delta) {
    asm("grow_memory $discard=, $0");
}

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    assert(_pageCount < maxPageCount);
    unsigned size = getMemorySize(), pad = reinterpret_cast<unsigned long>(Storage::heapBegin);
    size = (size > pad) ? (size-pad)/4096 : 0;
    if(_pageCount > size)
        growMemory(_pageCount-size);
    pageCount = _pageCount;
}

struct Main {
    Main() {
        Storage::heapBegin = reinterpret_cast<void*>(4096*10); // TODO
        Storage::resizeMemory(Storage::minPageCount);
        Ontology::tryToFillPreDefined();
    }
} startUp;
