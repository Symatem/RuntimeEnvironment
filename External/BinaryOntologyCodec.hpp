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

    enum BlobOption {
        BlobOptionRaw,
        BlobOptionArithmetic
    } blobOption = BlobOptionRaw;

    BinaryOntologyCodec() {
        blob = Blob(BinaryOntologyCodecSymbol);
    }
};

struct BinaryOntologyEncoder : public BinaryOntologyCodec {
    StaticHuffmanEncoder symbolHuffmanEncoder;
    Symbol symbolIndexOffset;

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
        BlobPairSet<false, Symbol> beta;
        beta.symbol = betaSymbol;
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
        BlobPairSet<false, Symbol> beta;
        beta.symbol = betaSymbol;

        encodeSymbol(doWrite, entity);
        if(doWrite) {
            encodeNatural(srcBlobLength);
            switch(blobOption) {
                case BlobOptionRaw:
                    blob.increaseSize(offset, srcBlobLength);
                    blob.interoperation(srcBlob, offset, 0, srcBlobLength);
                    offset += srcBlobLength;
                    break;
                case BlobOptionArithmetic:
                    arithmeticEncodeBlob(blob, offset, srcBlob, srcBlobLength);
                    break;
            }
        }

        if(doWrite)
            encodeNatural(beta.size());
        beta.iterateFirstKeys([&](Symbol attribute) {
            encodeAttribute(doWrite, beta.symbol, attribute);
        });
    }

    void encodeEntities(bool doWrite) {
        for(NativeNaturalType at = symbolIndexOffset; at < symbolIndexOffset+symbolsInChunk; ++at) {
            auto alphaResult = tripleIndex.readElementAt(at);
            encodeEntity(doWrite, alphaResult.first, alphaResult.second[EAV]);
        }
    }

    void encodeChunk() {
        encodeNatural(chunkOption);
        encodeNatural(numberOption);
        encodeNatural(symbolOption);
        encodeNatural(blobOption);
        if(symbolOption == SymbolOptionStaticHuffman) {
            encodeEntities(false);
            symbolHuffmanEncoder.encodeTree();
        }
        encodeNatural(symbolsInChunk);
        encodeEntities(true);
    }

    void encode() {
        symbolIndexOffset = 0;
        symbolsInChunk = tripleIndex.size();
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

    BinaryOntologyDecoder() :BinaryOntologyCodec(), symbolHuffmanDecoder(blob, offset) {
        symbolHuffmanDecoder.symbolVector.symbol = BinaryOntologyCodecAux0Symbol;
        symbolHuffmanDecoder.huffmanChildren.symbol = BinaryOntologyCodecAux1Symbol;
    }

    ~BinaryOntologyDecoder() {
        symbolHuffmanDecoder.symbolVector.symbol = VoidSymbol;
        symbolHuffmanDecoder.huffmanChildren.symbol = VoidSymbol;
    }

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
        NativeNaturalType dstBlobLength = decodeNatural();
        dstBlob.setSize(dstBlobLength);
        switch(blobOption) {
            case BlobOptionRaw:
                dstBlob.interoperation(blob, 0, offset, dstBlobLength);
                offset += dstBlobLength;
                break;
            case BlobOptionArithmetic:
                arithmeticDecodeBlob(dstBlob, dstBlobLength, blob, offset);
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
        blobOption = static_cast<BlobOption>(decodeNatural());
        if(symbolOption == SymbolOptionStaticHuffman) {
            symbolHuffmanDecoder.decodeTree();
            for(NativeNaturalType i = 0; i < symbolHuffmanDecoder.symbolCount; ++i) { // TODO
                Symbol symbol = symbolHuffmanDecoder.symbolVector.readElementAt(i);
                if(symbol > preDefinedSymbolsCount)
                    symbolHuffmanDecoder.symbolVector.writeElementAt(i, createSymbol());
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
