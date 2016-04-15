#include "../Interpreter/Procedures.hpp"

extern "C" {
    NativeNaturalType memcpy(NativeNaturalType dst, NativeNaturalType src, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            *reinterpret_cast<char*>(dst+i) = *reinterpret_cast<char*>(src+i);
        return 0;
    }

    NativeNaturalType memset(NativeNaturalType dst, NativeNaturalType value, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            *reinterpret_cast<char*>(dst+i) = value;
        return 0;
    }
};

__attribute__((noinline)) NativeNaturalType getMemorySize() {
    NativeNaturalType result;
    asm("memory_size $0=");
    return result;
}

__attribute__((noinline)) void growMemory(NativeNaturalType delta) {
    asm("grow_memory $discard=, $0");
}

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    assert(_pageCount < maxPageCount);
    NativeNaturalType size = getMemorySize(), pad = pointerToNatural(Storage::heapBegin);
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
