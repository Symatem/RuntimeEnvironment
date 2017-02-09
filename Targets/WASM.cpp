#include <External/HrlDeserialize.hpp>

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

EXPORT void decreaseBlobSize(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Blob(symbol).decreaseSize(offset, length);
}

EXPORT void increaseBlobSize(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Blob(symbol).decreaseSize(offset, length);
}

EXPORT void readBlob(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Natural8 buffer[4096];
    Blob(symbol).externalOperate<false>(buffer, offset, length);
}

EXPORT void writeBlob(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Natural8 buffer[4096];
    Blob(symbol).externalOperate<true>(buffer, offset, length);
}

EXPORT void chaCha20(Symbol dst, Symbol src) {
    ChaCha20 context;
    Blob(src).externalOperate<false>(&context, 0, sizeOfInBits<ChaCha20>::value);
    Blob(dst).chaCha20(context);
}

EXPORT Symbol deserializeHRL(Symbol inputSymbol, Symbol outputSymbol, Symbol packageSymbol) {
    HrlDeserializer deserializer;
    deserializer.queue.symbol = (outputSymbol == VoidSymbol) ? createSymbol() : outputSymbol;
    deserializer.input = inputSymbol;
    deserializer.package = packageSymbol;
    Symbol exception = deserializer.deserialize();
    if(outputSymbol == VoidSymbol)
        unlink(deserializer.queue.symbol);
    return exception;
}

EXPORT void encodeBinary() {
    BinaryEncoder encoder;
    encoder.encode();
}

EXPORT void decodeBinary() {
    BinaryDecoder decoder;
    decoder.decode();
}

EXPORT NativeNaturalType query(QueryMask mask, Symbol entity, Symbol attribute, Symbol value, Symbol resultSymbol) {
    QueryMode mode[3] = {
        static_cast<QueryMode>(mask%3),
        static_cast<QueryMode>((mask/3)%3),
        static_cast<QueryMode>((mask/9)%3)
    };
    BlobVector<true, Symbol> result;
    result.symbol = resultSymbol;
    auto count = query(mask, {entity, attribute, value}, [&](Triple triple) {
        for(NativeNaturalType i = 0; i < 3; ++i)
            if(mode[i] == Varying)
                result.push_back(triple.pos[i]);
    });
    if(resultSymbol)
        result.symbol = VoidSymbol;
    return count;
}

EXPORT bool link(Symbol entity, Symbol attribute, Symbol value) {
    return link({entity, attribute, value});
}

EXPORT bool unlink(Symbol entity, Symbol attribute, Symbol value) {
    return unlink({entity, attribute, value});
}

struct Main {
    Main() {
        setStackPointer(reinterpret_cast<NativeNaturalType>(stack)+sizeof(stack));
        superPage = reinterpret_cast<SuperPage*>(__builtin_wasm_current_memory()*bitsPerChunk/8);
        resizeMemory(minPageCount);
        tryToFillPreDefined();
    };
} main;

}
