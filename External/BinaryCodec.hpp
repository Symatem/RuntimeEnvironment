#include <Ontology/Triple.hpp>

struct BinaryCodec {
    Blob blob;
    NativeNaturalType offset;

    BinaryCodec(Symbol symbol) :blob(Blob(symbol)), offset(0) { }
};

struct BinaryEncoder : public BinaryCodec {
    BlobSet<true, Symbol, NativeNaturalType> symbolMap;

    void encodeSymbol(bool notDryRun, Symbol symbol) {
        NativeNaturalType indexInContainer;
        bool found = symbolMap.find(symbol, indexInContainer);
        if(notDryRun) {
            assert(found);
            blob.encodeBvlNatural(offset, symbolMap.value(indexInContainer));
        } else if(found)
            symbolMap.writeValueAt(indexInContainer, symbolMap.value(indexInContainer)+1);
        else
            symbolMap.insertElement({symbol, 1});
    }

    void encodeAttribute(bool notDryRun, Symbol attribute, Symbol gammaSymbol) {
        encodeSymbol(notDryRun, attribute);

        BlobSet<false, Symbol> gamma;
        gamma.symbol = gammaSymbol;
        if(notDryRun)
            blob.encodeBvlNatural(offset, gamma.size());
        gamma.iterateKeys([&](Symbol gammaResult) {
            encodeSymbol(notDryRun, gammaResult);
        });
    }

    void encodeEntity(bool notDryRun, Symbol entity, Symbol betaSymbol) {
        encodeSymbol(notDryRun, entity);

        if(notDryRun) {
            Blob srcBlob(entity);
            NativeNaturalType blobLength = srcBlob.getSize();
            blob.encodeBvlNatural(offset, blobLength);
            blob.increaseSize(offset, blobLength);
            blob.interoperation(srcBlob, offset, 0, blobLength);
            offset += blobLength;
        }

        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = betaSymbol;
        if(notDryRun)
            blob.encodeBvlNatural(offset, beta.size());
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            encodeAttribute(notDryRun, betaResult.key, betaResult.value);
        });
    }

    void encodeEntities(bool notDryRun) {
        tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
            encodeEntity(notDryRun, alphaResult.key, alphaResult.value[EAV]);
        });
    }

    void encode() {
        blob.setSize(0);
        encodeEntities(false);

        BlobHeap<true, NativeNaturalType, Symbol> symbolHeap;
        NativeNaturalType index = 0;
        symbolHeap.reserve(symbolMap.size());
        symbolMap.iterate([&](Pair<Symbol, NativeNaturalType> pair) {
            symbolHeap.writeElementAt(index++, {pair.value, pair.key});
        });
        symbolHeap.sort();

        index = 0;
        blob.encodeBvlNatural(offset, symbolHeap.size());
        symbolHeap.iterate([&](Pair<NativeNaturalType, Symbol> pair) {
            blob.encodeBvlNatural(offset, pair.value);
            NativeNaturalType indexInContainer;
            assert(symbolMap.find(pair.value, indexInContainer));
            auto element = symbolMap.readElementAt(indexInContainer);
            element.value = index++;
            symbolMap.writeElementAt(indexInContainer, element);
        });

        encodeEntities(true);
    }

    BinaryEncoder(Symbol symbol) :BinaryCodec(symbol) { }
};

struct BinaryDecoder : public BinaryCodec {
    BlobVector<true, Symbol> symbolVector;

    Symbol decodeSymbol() {
        Symbol symbol = blob.decodeBvlNatural(offset);
        symbol = symbolVector.readElementAt(symbol);
        if(superPage->symbolsEnd < symbol+1)
            superPage->symbolsEnd = symbol+1;
        return symbol;
    }

    void decodeAttribute(Symbol entity) {
        Symbol attribute = decodeSymbol();

        NativeNaturalType valueCount = blob.decodeBvlNatural(offset);
        for(NativeNaturalType i = 0; i < valueCount; ++i)
            link({entity, attribute, decodeSymbol()});
    }

    void decodeEntity() {
        Symbol entity = decodeSymbol();

        Blob dstBlob(entity);
        NativeNaturalType blobLength = blob.decodeBvlNatural(offset);
        dstBlob.setSize(blobLength);
        dstBlob.interoperation(blob, 0, offset, blobLength);
        offset += blobLength;

        NativeNaturalType attributeCount = blob.decodeBvlNatural(offset);
        for(NativeNaturalType i = 0; i < attributeCount; ++i)
            decodeAttribute(entity);
    }

    void decode() {
        NativeNaturalType symbolCount = blob.decodeBvlNatural(offset);
        symbolVector.reserve(symbolCount);
        for(NativeNaturalType i = 0; i < symbolCount; ++i) {
            Symbol symbol = blob.decodeBvlNatural(offset);
            symbolVector.writeElementAt(i, symbol);
        }

        superPage->symbolsEnd = 0;
        NativeNaturalType length = blob.getSize();
        while(offset < length)
            decodeEntity();
    }

    BinaryDecoder(Symbol symbol) :BinaryCodec(symbol) { }
};
