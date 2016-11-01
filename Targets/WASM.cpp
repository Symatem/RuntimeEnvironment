#include "../HRL/Deserialize.hpp"

const NativeNaturalType bitsPerChunk = 1<<19;

extern "C" {

// TODO: EXPORT some functions to be used from the outside

Natural8 stack[0x10000];

EXPORT void setStackPointer(NativeNaturalType ptr) {
    asm volatile(
        "i32.const $push0=, 0\n\t"
        "i32.store __stack_pointer($pop0), $0\n"
        : : "r"(ptr)
    );
}

EXPORT NativeNaturalType getStackPointer() {
    NativeNaturalType result;
    asm volatile(
        "i32.const $push0=, 0\n\t"
        "i32.load $0=, __stack_pointer($pop0)\n"
        : "=r"(result)
    );
    return result;
}

EXPORT void main() {
    setStackPointer(reinterpret_cast<NativeNaturalType>(stack)+sizeof(stack));
    Storage::superPage = reinterpret_cast<Storage::SuperPage*>(__builtin_wasm_current_memory()*bitsPerChunk/8);
    Storage::resizeMemory(Storage::minPageCount);
    Ontology::tryToFillPreDefined();
}

}

void assertFailed(const char* str) {
    __builtin_unreachable();
}

void Storage::resizeMemory(NativeNaturalType pagesEnd) {
    NativeNaturalType newChunkCount = (pagesEnd*Storage::bitsPerPage+bitsPerChunk-1)/bitsPerChunk,
                      size = __builtin_wasm_current_memory()*bitsPerChunk,
                      pad = reinterpret_cast<NativeNaturalType>(Storage::superPage)*8,
                      oldChunkCount = (size > pad) ? (size-pad+bitsPerChunk-1)/bitsPerChunk : 0;
    if(newChunkCount > oldChunkCount)
        __builtin_wasm_grow_memory(newChunkCount-oldChunkCount);
    superPage->pagesEnd = pagesEnd;
}
