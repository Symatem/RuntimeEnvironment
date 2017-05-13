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
        tryToFillPreDefined();
    };
} main;

EXPORT Symbol _createSymbol() {
    return createSymbol();
}

EXPORT void _releaseSymbol(Symbol symbol) {
    releaseSymbol(symbol);
}

EXPORT NativeNaturalType getBlobSize(Symbol symbol) {
    return Blob(symbol).getSize();
}

EXPORT void setBlobSize(Symbol symbol, NativeNaturalType size) {
    Blob(symbol).setSize(size);
}

EXPORT bool decreaseBlobSize(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    return Blob(symbol).decreaseSize(offset, length);
}

EXPORT bool increaseBlobSize(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    return Blob(symbol).increaseSize(offset, length);
}

EXPORT bool readBlob(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Natural8 buffer[4096];
    return Blob(symbol).externalOperate<false>(buffer, offset, length);
}

EXPORT bool writeBlob(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Natural8 buffer[4096];
    return Blob(symbol).externalOperate<true>(buffer, offset, length);
}

EXPORT void chaCha20(Symbol dst, Symbol src) {
    ChaCha20 context;
    Blob(src).externalOperate<false>(&context, 0, sizeOfInBits<ChaCha20>::value);
    Blob(dst).chaCha20(context);
}

EXPORT bool link(Symbol entity, Symbol attribute, Symbol value) {
    return link({entity, attribute, value});
}

EXPORT bool unlink(Symbol entity, Symbol attribute, Symbol value) {
    return unlink({entity, attribute, value});
}

EXPORT NativeNaturalType query(QueryMask mask, Symbol entity, Symbol attribute, Symbol value, Symbol resultSymbol) {
    QueryMode mode[3] = {
        static_cast<QueryMode>(mask%3),
        static_cast<QueryMode>((mask/3)%3),
        static_cast<QueryMode>((mask/9)%3)
    };
    DataStructure<Vector<Symbol>> result((resultSymbol == VoidSymbol) ? createSymbol() : resultSymbol);
    auto count = query(mask, {entity, attribute, value}, [&](Triple triple) {
        for(NativeNaturalType i = 0; i < 3; ++i)
            if(mode[i] == Varying)
                insertAsLastElement(result, triple.pos[i]);
    });
    if(resultSymbol != VoidSymbol)
        unlink(result.getBitVector().symbol);
    return count;
}

EXPORT void encodeOntologyBinary() {
    BinaryOntologyEncoder encoder;
    encoder.encode();
}

EXPORT void decodeOntologyBinary() {
    BinaryOntologyDecoder decoder;
    decoder.decode();
}

}
