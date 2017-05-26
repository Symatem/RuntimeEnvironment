#include <External/BinaryOntologyCodec.hpp>

const NativeNaturalType bitsPerChunk = 1<<19;

void resizeMemory(NativeNaturalType pagesEnd) {
    NativeNaturalType newChunkCount = (pagesEnd*bitsPerPage+bitsPerChunk-1)/bitsPerChunk,
                      size = __builtin_wasm_current_memory()*bitsPerChunk,
                      pad = reinterpret_cast<NativeNaturalType>(superPage)*8,
                      oldChunkCount = (size > pad) ? (size-pad+bitsPerChunk-1)/bitsPerChunk : 0;
    if(newChunkCount > oldChunkCount)
        __builtin_wasm_grow_memory(newChunkCount-oldChunkCount);
    superPage->pagesEnd = pagesEnd;
}

extern "C" {

Natural8 stack[bitsPerChunk/8];

IMPORT void consoleLogString(const Integer8* basePtr, Natural32 length);
IMPORT void consoleLogInteger(NativeNaturalType value);
IMPORT void consoleLogFloat(Float64 value);

void puts(const Integer8* message) {
    consoleLogString(message, strlen(message));
}

void assertFailed(const char* str) {
    puts(str);
    asm volatile("unreachable");
    __builtin_unreachable();
}

DO_NOT_INLINE EXPORT void setStackPointer(NativeNaturalType ptr) {
    asm volatile(
        "i32.const $push0=, 0\n\t"
        "i32.store __stack_pointer($pop0), $0\n"
        : : "r"(ptr)
    );
}

DO_NOT_INLINE EXPORT NativeNaturalType getStackPointer() {
    NativeNaturalType result;
    asm volatile(
        "i32.const $push0=, 0\n\t"
        "i32.load $0=, __stack_pointer($pop0)\n"
        : "=r"(result)
    );
    return result;
}

struct Main {
    Main() {
        setStackPointer(reinterpret_cast<NativeNaturalType>(stack)+sizeof(stack));
        superPage = reinterpret_cast<SuperPage*>(__builtin_wasm_current_memory()*bitsPerChunk/8);
        resizeMemory(minPageCount);
        superPage->init();
    };
} main;

// TODO

}
