#include "../Interpreter/Procedures.hpp"

extern "C" {

    // TODO: EXPORT some functions to be used from the outside

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

    void assertFailed(const char* str) {
        asm("unreachable");
    }

    DO_NOT_INLINE NativeNaturalType getMemorySize() {
        asm("memory_size $push0=\n"
            "\treturn $pop0");
        __builtin_unreachable();
    }

    DO_NOT_INLINE void growMemory(NativeNaturalType delta) {
        asm("grow_memory $discard=, $0");
    }
};

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    assert(_pageCount < maxPageCount);
    NativeNaturalType size = getMemorySize(), pad = pointerToNatural(Storage::heapBegin);
    size = (size > pad) ? (size-pad)/4096 : 0;
    // TODO: Page size is 0x10000
    if(_pageCount > size)
        growMemory(_pageCount-size);
    pageCount = _pageCount;
}

struct Main {
    Main() {
        Storage::heapBegin = reinterpret_cast<void*>(getMemorySize());
        Storage::resizeMemory(Storage::minPageCount);
        Ontology::tryToFillPreDefined();
    }
} startUp;
