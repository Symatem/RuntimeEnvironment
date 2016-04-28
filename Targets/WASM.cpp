#include "../Interpreter/Primitives.hpp"

extern "C" {

    // TODO: EXPORT some functions to be used from the outside

    NativeNaturalType memcpy(void* dst, void* src, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            reinterpret_cast<char*>(dst)[i] = reinterpret_cast<char*>(src)[i];
        return 0;
    }

    NativeNaturalType memset(void* dst, NativeNaturalType value, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            reinterpret_cast<char*>(dst)[i] = value;
        return 0;
    }

    void assertFailed(const char* str) {
        asm("unreachable");
    }

    DO_NOT_INLINE NativeNaturalType getMemorySize() {
        volatile NativeNaturalType result;
        asm("memory_size $push0=\n\treturn $pop0");
        return result = 0;
    }

    DO_NOT_INLINE void growMemory(NativeNaturalType delta) {
        asm("grow_memory $discard=, $0");
    }
};

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    assert(_pageCount < maxPageCount);
    pageCount = _pageCount;
    const NativeNaturalType bitsPerChunk = 2<<19;
    NativeNaturalType _chunkCount = (pageCount*Storage::bitsPerPage+bitsPerChunk-1)/bitsPerChunk;
    NativeNaturalType size = getMemorySize()*8, pad = pointerToNatural(Storage::heapBegin)*8;
    NativeNaturalType chunkCount = (size > pad) ? (size-pad+bitsPerChunk-1)/bitsPerChunk : 0;
    if(_chunkCount > chunkCount)
        growMemory(_chunkCount-chunkCount);
}

struct Main {
    Main() {
        Storage::heapBegin = reinterpret_cast<void*>(getMemorySize());
        Storage::resizeMemory(Storage::minPageCount);
        Ontology::tryToFillPreDefined();
    }
} startUp;
