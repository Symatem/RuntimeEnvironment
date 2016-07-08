#include "../Interpreter/Primitives.hpp"

const NativeNaturalType bitsPerChunk = 1<<19;

extern "C" {

// TODO: EXPORT some functions to be used from the outside

EXPORT NativeNaturalType getSuperPageByteAddress() {
    return reinterpret_cast<NativeNaturalType>(Storage::superPage);
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

}

void Storage::resizeMemory(NativeNaturalType pagesEnd) {
    assert(pagesEnd < maxPageCount);
    superPage->pagesEnd = pagesEnd;
    NativeNaturalType newChunkCount = (pagesEnd*Storage::bitsPerPage+bitsPerChunk-1)/bitsPerChunk,
                      size = __builtin_wasm_current_memory()*bitsPerChunk,
                      pad = getSuperPageByteAddress()*8,
                      oldChunkCount = (size > pad) ? (size-pad+bitsPerChunk-1)/bitsPerChunk : 0;
    if(newChunkCount > oldChunkCount)
        __builtin_wasm_grow_memory(newChunkCount-oldChunkCount);
}
