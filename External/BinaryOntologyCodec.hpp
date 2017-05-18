#include <External/Arithmetic.hpp>

struct BinaryOntologyCodec {
    Blob blob;
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
        blob = Blob(BinaryOntologyCodecSymbol);
    }
};

struct BinaryOntologyEncoder : public BinaryOntologyCodec {
    StaticHuffmanEncoder symbolHuffmanEncoder;
    Symbol symbolIndexOffset;
    NativeNaturalType emptySymbolsInChunk;

    BinaryOntologyEncoder() :BinaryOntologyCodec(), symbolHuffmanEncoder(blob, offset) {
        blob.setSize(0);
    }

    void encodeNatural(NativeNaturalType value) {
        switch(numberOption) {
            case NumberOptionRaw:
                blob.increaseSize(offset, naturalLength);
                blob.externalOperate<true>(&value, offset, naturalLength);
                offset += naturalLength;
                break;
            case NumberOptionBinaryVariableLength:
                encodeBvlNatural(blob, offset, value);
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

    void encodeEntity(bool doWrite, Symbol entity, Symbol betaSymbol) {
        Blob srcBlob(entity);
        NativeNaturalType srcBlobLength = srcBlob.getSize();
        DataStructure<PairSet<Symbol, Symbol>> beta(betaSymbol);
        if(srcBlobLength == 0 && beta.isEmpty()) {
            ++emptySymbolsInChunk;
            return;
        }

        encodeSymbol(doWrite, entity);
        if(doWrite) {
            if(srcBlobLength == 0)
                encodeNatural(0);
            else {
                // TODO: Sparse BitMap Support
                encodeNatural(1); // Slice count: 1
                encodeNatural(0); // 1. Slice offset: 0
                encodeNatural(srcBlobLength); // 1. Slice length
                switch(bitMapOption) {
                    case BitMapOptionRaw:
                        blob.increaseSize(offset, srcBlobLength);
                        blob.interoperation(srcBlob, offset, 0, srcBlobLength);
                        offset += srcBlobLength;
                        break;
                    case BitMapOptionArithmetic:
                        arithmeticEncodeBlob(blob, offset, srcBlob, srcBlobLength);
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
            auto alphaResult = tripleIndex.getElementAt(at);
            encodeEntity(doWrite, alphaResult.first, alphaResult.second.subIndices[EAV]);
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
        blob.increaseSize(blob.getSize(), padding);
        blob.increaseSize(0, 8);
        blob.externalOperate<true>(&padding, 0, 8);
        offset += 8+padding;

        blob.increaseSize(0, headerLength);
        blob.externalOperate<true>(&superPage->version, 0, headerLength);
        offset += headerLength;
    }
};

struct BinaryOntologyDecoder : public BinaryOntologyCodec {
    StaticHuffmanDecoder symbolHuffmanDecoder;

    BinaryOntologyDecoder() :BinaryOntologyCodec(), symbolHuffmanDecoder(blob, offset) {}

    NativeNaturalType decodeNatural() {
        NativeNaturalType value;
        switch(numberOption) {
            case NumberOptionRaw:
                blob.externalOperate<false>(&value, offset, naturalLength);
                offset += naturalLength;
                break;
            case NumberOptionBinaryVariableLength:
                value = decodeBvlNatural(blob, offset);
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
        Blob dstBlob(entity);
        NativeNaturalType sliceCount = decodeNatural(), sliceOffset, sliceLength;
        if(sliceCount == 0) {
            sliceOffset = 0;
            sliceLength = 0;
        } else { // TODO: Sparse BitMap Support
            sliceOffset = decodeNatural();
            sliceLength = decodeNatural();
        }
        dstBlob.setSize(sliceLength);
        switch(bitMapOption) {
            case BitMapOptionRaw:
                dstBlob.interoperation(blob, 0, offset, sliceLength);
                offset += sliceLength;
                break;
            case BitMapOptionArithmetic:
                arithmeticDecodeBlob(dstBlob, sliceLength, blob, offset);
                break;
            default:
                assert(false);
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
        blob.externalOperate<false>(&naturalLength, headerLength-8, 8);
        naturalLength = 1<<naturalLength;
        assert(naturalLength <= architectureSize);
        offset = headerLength;
        Natural8 padding;
        blob.externalOperate<false>(&padding, offset, 8);
        NativeNaturalType blobLength = blob.getSize()-padding;
        offset += 8;
        while(offset < blobLength)
            decodeChunk();
    }
};
