#include <External/Arithmetic.hpp>

struct BinaryCodec {
    Blob blob;
    NativeNaturalType offset = 0, naturalLength = architectureSize, symbolsInChunk;
    const static NativeNaturalType headerLength = 64+8*45;

    enum NumberCodec {
        NumberCodecRaw,
        NumberCodecBinaryVariableLength
    } numberCodec = NumberCodecBinaryVariableLength;

    enum SymbolCodec {
        SymbolCodecNatural,
        SymbolCodecHuffman
    } symbolCodec = SymbolCodecHuffman;

    enum BlobCodec {
        BlobCodecRaw
    } blobCodec = BlobCodecRaw;

    BinaryCodec() {
        blob = Blob(BinaryCodecSymbol);
    }
};

struct BinaryEncoder : public BinaryCodec {
    StaticHuffmanEncoder symbolHuffmanEncoder;
    Symbol symbolIndexOffset;

    void encodeNatural(NativeNaturalType value) {
        switch(numberCodec) {
            case NumberCodecRaw:
                blob.increaseSize(offset, naturalLength);
                blob.externalOperate<true>(&value, offset, naturalLength);
                offset += naturalLength;
                break;
            case NumberCodecBinaryVariableLength:
                encodeBvlNatural(blob, offset, value);
                break;
        }
    }

    void encodeSymbol(bool doWrite, Symbol symbol) {
        switch(symbolCodec) {
            case SymbolCodecNatural:
                encodeNatural(symbol);
                break;
            case SymbolCodecHuffman:
                if(doWrite)
                    symbolHuffmanEncoder.encodeSymbol(symbol);
                else
                    symbolHuffmanEncoder.countSymbol(symbol);
                break;
        }
    }

    void encodeAttribute(bool doWrite, Symbol attribute, Symbol gammaSymbol) {
        encodeSymbol(doWrite, attribute);
        BlobSet<false, Symbol> gamma;
        gamma.symbol = gammaSymbol;
        if(doWrite)
            encodeNatural(gamma.size()-1);
        gamma.iterateKeys([&](Symbol gammaResult) {
            encodeSymbol(doWrite, gammaResult);
        });
    }

    void encodeEntity(bool doWrite, Symbol entity, Symbol betaSymbol) {
        Blob srcBlob(entity);
        NativeNaturalType blobLength = srcBlob.getSize();
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = betaSymbol;

        encodeSymbol(doWrite, entity);
        if(doWrite)
            switch(blobCodec) {
                case BlobCodecRaw: {
                    encodeNatural(blobLength);
                    blob.increaseSize(offset, blobLength);
                    blob.interoperation(srcBlob, offset, 0, blobLength);
                    offset += blobLength;
                } break;
            }

        if(doWrite)
            encodeNatural(beta.size());
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            encodeAttribute(doWrite, betaResult.key, betaResult.value);
        });
    }

    void encodeEntities(bool doWrite) {
        for(NativeNaturalType at = symbolIndexOffset; at < symbolIndexOffset+symbolsInChunk; ++at) {
            auto alphaResult = tripleIndex.readElementAt(at);
            encodeEntity(doWrite, alphaResult.key, alphaResult.value[EAV]);
        }
    }

    void encodeChunk() {
        encodeNatural(numberCodec);
        encodeNatural(symbolCodec);
        encodeNatural(blobCodec);
        if(symbolCodec == SymbolCodecHuffman) {
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

    BinaryEncoder() :BinaryCodec(), symbolHuffmanEncoder(blob, offset) {
        symbolHuffmanEncoder.symbolMap.symbol = BinaryCodecAux0Symbol;
        symbolHuffmanEncoder.huffmanCodes = Blob(BinaryCodecAux1Symbol);
        blob.setSize(0);
    }
};

struct BinaryDecoder : public BinaryCodec {
    StaticHuffmanDecoder symbolHuffmanDecoder;

    NativeNaturalType decodeNatural() {
        NativeNaturalType value;
        switch(numberCodec) {
            case NumberCodecRaw:
                blob.externalOperate<false>(&value, offset, naturalLength);
                offset += naturalLength;
                break;
            case NumberCodecBinaryVariableLength:
                value = decodeBvlNatural(blob, offset);
                break;
            default:
                assert(false);
        }
        return value;
    }

    Symbol decodeSymbol() {
        Symbol symbol;
        switch(symbolCodec) {
            case SymbolCodecNatural:
                symbol = decodeNatural();
                break;
            case SymbolCodecHuffman:
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
        switch(blobCodec) {
            case BlobCodecRaw: {
                NativeNaturalType blobLength = decodeNatural();
                dstBlob.setSize(blobLength);
                dstBlob.interoperation(blob, 0, offset, blobLength);
                offset += blobLength;
            } break;
            default:
                assert(false);
        }
        NativeNaturalType attributeCount = decodeNatural();
        for(NativeNaturalType i = 0; i < attributeCount; ++i)
            decodeAttribute(entity);
    }

    void decodeChunk() {
        numberCodec = static_cast<NumberCodec>(decodeNatural());
        symbolCodec = static_cast<SymbolCodec>(decodeNatural());
        blobCodec = static_cast<BlobCodec>(decodeNatural());
        if(symbolCodec == SymbolCodecHuffman) {
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

    BinaryDecoder() :BinaryCodec(), symbolHuffmanDecoder(blob, offset) {
        symbolHuffmanDecoder.symbolVector.symbol = BinaryCodecAux0Symbol;
        symbolHuffmanDecoder.huffmanChildren.symbol = BinaryCodecAux1Symbol;
    }
};
