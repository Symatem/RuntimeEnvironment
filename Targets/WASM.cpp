#include "../Interpreter/Primitives.hpp"

const NativeNaturalType bitsPerChunk = 2<<19;

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
        __builtin_unreachable();
    }

    struct Main {
        Main() {
            Storage::superPage = reinterpret_cast<Storage::SuperPage*>(__builtin_wasm_current_memory()*bitsPerChunk/8);
            Storage::resizeMemory(Storage::minPageCount);
            Ontology::tryToFillPreDefined();
        }
    } startUp;
};

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    assert(_pageCount < maxPageCount);
    pageCount = _pageCount;
    NativeNaturalType _chunkCount = (pageCount*Storage::bitsPerPage+bitsPerChunk-1)/bitsPerChunk;
    NativeNaturalType size = __builtin_wasm_current_memory()*bitsPerChunk, pad = pointerToNatural(Storage::superPage)*8;
    NativeNaturalType chunkCount = (size > pad) ? (size-pad+bitsPerChunk-1)/bitsPerChunk : 0;
    if(_chunkCount > chunkCount)
        __builtin_wasm_grow_memory(_chunkCount-chunkCount);
}
