#include <External/Arithmetic.hpp>

struct BinaryOntologyCodec {
    BitVector bitVector;
    NativeNaturalType offset = 0, naturalLength = architectureSize, symbolsInChunk;
    const static NativeNaturalType headerLength = 64+8*45;

    enum ChunkOption {
        ChunkOptionRaw
    } chunkOption = ChunkOptionRaw;

    enum NumberOption {
        NumberOptionRaw,
        NumberOptionBinaryVariableLength
    } numberOption = NumberOptionBinaryVariableLength;

    enum SymbolOption {
        SymbolOptionNatural,
        SymbolOptionStaticHuffman
    } symbolOption = SymbolOptionStaticHuffman;

    enum BitMapOption {
        BitMapOptionRaw,
        BitMapOptionArithmetic
    } bitMapOption = BitMapOptionRaw;

    BinaryOntologyCodec() {
        bitVector = BitVector(BinaryOntologyCodecSymbol);
    }
};

struct BinaryOntologyEncoder : public BinaryOntologyCodec {
    StaticHuffmanEncoder symbolHuffmanEncoder;
    Symbol symbolIndexOffset;
    NativeNaturalType emptySymbolsInChunk;

    BinaryOntologyEncoder() :BinaryOntologyCodec(), symbolHuffmanEncoder(bitVector, offset) {
        bitVector.setSize(0);
    }

    void encodeNatural(NativeNaturalType value) {
        switch(numberOption) {
            case NumberOptionRaw:
                bitVector.increaseSize(offset, naturalLength);
                bitVector.externalOperate<true>(&value, offset, naturalLength);
                offset += naturalLength;
                break;
            case NumberOptionBinaryVariableLength:
                encodeBvlNatural(bitVector, offset, value);
                break;
        }
    }

    void encodeSymbol(bool doWrite, Symbol symbol) {
        switch(symbolOption) {
            case SymbolOptionNatural:
                if(doWrite)
                    encodeNatural(symbol);
                break;
            case SymbolOptionStaticHuffman:
                if(doWrite)
                    symbolHuffmanEncoder.encodeSymbol(symbol);
                else
                    symbolHuffmanEncoder.countSymbol(symbol);
                break;
        }
    }

    void encodeAttribute(bool doWrite, Symbol betaSymbol, Symbol attribute) {
        NativeNaturalType secondAt;
        DataStructure<PairSet<Symbol, Symbol>> beta(betaSymbol);
        beta.findFirstKey(attribute, secondAt);
        encodeSymbol(doWrite, attribute);
        if(doWrite)
            encodeNatural(beta.getSecondKeyCount(secondAt)-1);
        beta.iterateSecondKeys(secondAt, [&](Symbol gammaResult) {
            encodeSymbol(doWrite, gammaResult);
        });
    }

    void encodeEntity(bool doWrite, Symbol entity, OntologyStruct& alpha) {
        auto beta = alpha.getSubIndex(EAV);
        auto bitMap = alpha.getBitMap();
        if(beta.isEmpty() && bitMap.isEmpty()) {
            ++emptySymbolsInChunk;
            return;
        }
        encodeSymbol(doWrite, entity);
        if(doWrite) {
            encodeNatural(bitMap.getElementCount());
            for(NativeNaturalType sliceIndex = 0; sliceIndex < bitMap.getElementCount(); ++sliceIndex) {
                NativeNaturalType sliceLength = bitMap.getChildLength(sliceIndex),
                                  sliceOffset = bitMap.getChildOffset(sliceIndex);
                encodeNatural(bitMap.getKeyAt(sliceIndex));
                encodeNatural(sliceLength);
                switch(bitMapOption) {
                    case BitMapOptionRaw:
                        bitVector.increaseSize(offset, sliceLength);
                        bitVector.interoperation(bitMap.getBitVector(), offset, sliceOffset, sliceLength);
                        offset += sliceLength;
                        break;
                    case BitMapOptionArithmetic:
                        arithmeticEncodeBitVector(bitVector, offset, bitMap.getBitVector(), sliceOffset, sliceLength);
                        break;
                }
            }
        }
        if(doWrite)
            encodeNatural(beta.getFirstKeyCount());
        beta.iterateFirstKeys([&](Symbol attribute) {
            encodeAttribute(doWrite, beta.getBitVector().symbol, attribute);
        });
    }

    void encodeEntities(bool doWrite) {
        for(NativeNaturalType at = symbolIndexOffset, end = symbolIndexOffset+symbolsInChunk; at < end; ++at) {
            auto element = tripleIndex.getElementAt(at);
            encodeEntity(doWrite, element.first, element.second);
        }
    }

    void encodeChunk() {
        encodeNatural(chunkOption);
        encodeNatural(numberOption);
        encodeNatural(symbolOption);
        encodeNatural(bitMapOption);
        emptySymbolsInChunk = 0;
        encodeEntities(false);
        if(symbolOption == SymbolOptionStaticHuffman)
            symbolHuffmanEncoder.encodeTree();
        encodeNatural(symbolsInChunk-emptySymbolsInChunk);
        encodeEntities(true);
    }

    void encode() {
        symbolIndexOffset = 0;
        symbolsInChunk = tripleIndex.getElementCount();
        encodeChunk();

        Natural8 padding = 8-offset%8;
        bitVector.increaseSize(bitVector.getSize(), padding);
        bitVector.increaseSize(0, 8);
        bitVector.externalOperate<true>(&padding, 0, 8);
        offset += 8+padding;

        bitVector.increaseSize(0, headerLength);
        bitVector.externalOperate<true>(&superPage->version, 0, headerLength);
        offset += headerLength;
    }
};

struct BinaryOntologyDecoder : public BinaryOntologyCodec {
    StaticHuffmanDecoder symbolHuffmanDecoder;

    BinaryOntologyDecoder() :BinaryOntologyCodec(), symbolHuffmanDecoder(bitVector, offset) {}

    NativeNaturalType decodeNatural() {
        NativeNaturalType value;
        switch(numberOption) {
            case NumberOptionRaw:
                bitVector.externalOperate<false>(&value, offset, naturalLength);
                offset += naturalLength;
                break;
            case NumberOptionBinaryVariableLength:
                value = decodeBvlNatural(bitVector, offset);
                break;
            default:
                assert(false);
        }
        return value;
    }

    Symbol decodeSymbol() {
        Symbol symbol;
        switch(symbolOption) {
            case SymbolOptionNatural:
                symbol = decodeNatural();
                break;
            case SymbolOptionStaticHuffman:
                symbol = symbolHuffmanDecoder.decodeSymbol();
                break;
            default:
                assert(false);
        }
        return symbol;
    }

    void decodeAttribute(Symbol entity) {
        Symbol attribute = decodeSymbol();
        NativeNaturalType valueCount = decodeNatural()+1;
        for(NativeNaturalType i = 0; i < valueCount; ++i) {
            Symbol value = decodeSymbol();
            link({entity, attribute, value});
        }
    }

    void decodeEntity() {
        Symbol entity = decodeSymbol();
        NativeNaturalType sliceCount = decodeNatural();
        auto bitMap = getOntologyStructOfSymbol(entity).getBitMap();
        bitMap.setElementCount(sliceCount);
        for(NativeNaturalType sliceIndex = 0; sliceIndex < sliceCount; ++sliceIndex) {
            bitMap.setKeyAt(sliceIndex, decodeNatural());
            NativeNaturalType sliceOffset = bitMap.getChildOffset(sliceIndex),
                              sliceLength = decodeNatural();
            bitMap.increaseChildLength(sliceIndex, sliceOffset, sliceLength);
            switch(bitMapOption) {
                case BitMapOptionRaw:
                    bitMap.getBitVector().interoperation(bitVector, sliceOffset, offset, sliceLength);
                    offset += sliceLength;
                    break;
                case BitMapOptionArithmetic:
                    arithmeticDecodeBitVector(bitMap.getBitVector(), sliceOffset, sliceLength, bitVector, offset);
                    break;
                default:
                    assert(false);
            }
        }
        NativeNaturalType attributeCount = decodeNatural();
        for(NativeNaturalType i = 0; i < attributeCount; ++i)
            decodeAttribute(entity);
    }

    void decodeChunk() {
        chunkOption = static_cast<ChunkOption>(decodeNatural());
        numberOption = static_cast<NumberOption>(decodeNatural());
        symbolOption = static_cast<SymbolOption>(decodeNatural());
        bitMapOption = static_cast<BitMapOption>(decodeNatural());
        if(symbolOption == SymbolOptionStaticHuffman) {
            symbolHuffmanDecoder.decodeTree();
            for(NativeNaturalType i = 0; i < symbolHuffmanDecoder.symbolCount; ++i) { // TODO
                Symbol symbol = symbolHuffmanDecoder.symbolVector.getElementAt(i);
                if(symbol >= preDefinedSymbolsCount)
                    symbolHuffmanDecoder.symbolVector.setElementAt(i, createSymbol());
            }
        }
        symbolsInChunk = decodeNatural();
        for(NativeNaturalType i = 0; i < symbolsInChunk; ++i)
            decodeEntity();
    }

    void decode() {
        bitVector.externalOperate<false>(&naturalLength, headerLength-8, 8);
        naturalLength = 1<<naturalLength;
        assert(naturalLength <= architectureSize);
        offset = headerLength;
        Natural8 padding;
        bitVector.externalOperate<false>(&padding, offset, 8);
        NativeNaturalType bitVectorLength = bitVector.getSize()-padding;
        offset += 8;
        while(offset < bitVectorLength)
            decodeChunk();
    }
};
