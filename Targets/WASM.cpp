#include "../Interpreter/Primitives.hpp"

const NativeNaturalType bitsPerChunk = 2<<19;

extern "C" {

// TODO: EXPORT some functions to be used from the outside

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

void Storage::resizeMemory(NativeNaturalType _pagesEnd) {
    assert(_pagesEnd < maxPageCount);
    superPage->pagesEnd = _pagesEnd;
    NativeNaturalType _chunkCount = (_pagesEnd*Storage::bitsPerPage+bitsPerChunk-1)/bitsPerChunk,
                      size = __builtin_wasm_current_memory()*bitsPerChunk,
                      pad = pointerToNatural(Storage::superPage)*8,
                      chunkCount = (size > pad) ? (size-pad+bitsPerChunk-1)/bitsPerChunk : 0;
    if(_chunkCount > chunkCount)
        __builtin_wasm_grow_memory(_chunkCount-chunkCount);
}
