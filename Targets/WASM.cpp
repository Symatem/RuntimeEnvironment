#include <HRL/Deserialize.hpp>

const NativeNaturalType bitsPerChunk = 1<<19;

void Storage::resizeMemory(NativeNaturalType pagesEnd) {
    NativeNaturalType newChunkCount = (pagesEnd*Storage::bitsPerPage+bitsPerChunk-1)/bitsPerChunk,
                      size = __builtin_wasm_current_memory()*bitsPerChunk,
                      pad = reinterpret_cast<NativeNaturalType>(Storage::superPage)*8,
                      oldChunkCount = (size > pad) ? (size-pad+bitsPerChunk-1)/bitsPerChunk : 0;
    if(newChunkCount > oldChunkCount)
        __builtin_wasm_grow_memory(newChunkCount-oldChunkCount);
    superPage->pagesEnd = pagesEnd;
}

extern "C" {

Natural8 stack[bitsPerChunk/8];

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

void assertFailed(const char* str) {
    __builtin_unreachable();
}

struct Main {
    Main() {
        setStackPointer(reinterpret_cast<NativeNaturalType>(stack)+sizeof(stack));
        Storage::superPage = reinterpret_cast<Storage::SuperPage*>(__builtin_wasm_current_memory()*bitsPerChunk/8);
        Storage::resizeMemory(Storage::minPageCount);
        Ontology::tryToFillPreDefined();
    };
} main;

EXPORT Symbol createSymbol() {
    return Storage::createSymbol();
}

EXPORT void releaseSymbol(Symbol symbol) {
    Ontology::unlink(symbol);
}

EXPORT NativeNaturalType getBlobSize(Symbol symbol) {
    return Storage::Blob(symbol).getSize();
}

EXPORT void setBlobSize(Symbol symbol, NativeNaturalType size) {
    Storage::Blob(symbol).setSize(size);
}

EXPORT void decreaseBlobSize(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Storage::Blob(symbol).decreaseSize(offset, length);
}

EXPORT void increaseBlobSize(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Storage::Blob(symbol).decreaseSize(offset, length);
}

EXPORT void readBlob(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Natural8 buffer[4096];
    Storage::Blob(symbol).externalOperate<false>(buffer, offset, length);
}

EXPORT void writeBlob(Symbol symbol, NativeNaturalType offset, NativeNaturalType length) {
    Natural8 buffer[4096];
    Storage::Blob(symbol).externalOperate<true>(buffer, offset, length);
}

EXPORT Symbol deserializeBlob(Symbol inputSymbol, Symbol outputSymbol, Symbol packageSymbol) {
    Deserializer deserializer;
    deserializer.input = inputSymbol;
    deserializer.queue.symbol = outputSymbol;
    deserializer.package = packageSymbol;
    Symbol exception = deserializer.deserialize();
    if(outputSymbol != Ontology::VoidSymbol)
        deserializer.queue.symbol = Ontology::VoidSymbol;
    return exception;
}

EXPORT NativeNaturalType query(Ontology::QueryMask mask, Symbol entity, Symbol attribute, Symbol value, Symbol resultSymbol) {
    Ontology::QueryMode mode[3] = {
        static_cast<Ontology::QueryMode>(mask%3),
        static_cast<Ontology::QueryMode>((mask/3)%3),
        static_cast<Ontology::QueryMode>((mask/9)%3)
    };
    Ontology::BlobVector<true, Symbol> result;
    result.symbol = resultSymbol;
    auto count = Ontology::query(mask, {entity, attribute, value}, [&](Ontology::Triple triple) {
        for(NativeNaturalType i = 0; i < 3; ++i)
            if(mode[i] == Ontology::Varying)
                result.push_back(triple.pos[i]);
    });
    if(resultSymbol)
        result.symbol = Ontology::VoidSymbol;
    return count;
}

EXPORT bool link(Symbol entity, Symbol attribute, Symbol value) {
    return Ontology::link({entity, attribute, value});
}

EXPORT bool unlink(Symbol entity, Symbol attribute, Symbol value) {
    return Ontology::unlink({entity, attribute, value});
}

}
