#include <Ontology/Triple.hpp>

struct BinaryCodec {
    Blob blob;
    NativeNaturalType offset = 0, naturalLength = architectureSize, symbolCount = 0;
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
    BlobSet<false, Symbol, NativeNaturalType> symbolMap;
    Blob huffmanCodes;
    Symbol symbolIndexOffset;

    void encodeNatural(NativeNaturalType value) {
        switch(numberCodec) {
            case NumberCodecRaw:
                blob.increaseSize(offset, naturalLength);
                blob.externalOperate<true>(&value, offset, naturalLength);
                offset += naturalLength;
                break;
            case NumberCodecBinaryVariableLength:
                blob.encodeBvlNatural(offset, value);
                break;
        }
    }

    void encodeSymbol(bool doWrite, Symbol symbol) {
        switch(symbolCodec) {
            case SymbolCodecNatural:
                encodeNatural(symbol);
                break;
            case SymbolCodecHuffman: {
                NativeNaturalType index;
                bool found = symbolMap.find(symbol, index);
                if(doWrite) {
                    assert(found);
                    if(symbolCount < 2)
                        return;
                    NativeNaturalType begin = symbolMap.readValueAt(index),
                                      length = (index+1 < symbolCount) ? symbolMap.readValueAt(index+1)-begin : huffmanCodes.getSize()-begin;
                    blob.increaseSize(offset, length);
                    blob.interoperation(huffmanCodes, offset, begin, length);
                    offset += length;
                } else if(found)
                    symbolMap.writeValueAt(index, symbolMap.readValueAt(index)+1);
                else
                    symbolMap.insertElement({symbol, 1});
            } break;
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

    void encodeBlob(Symbol symbol) {
        Blob srcBlob(symbol);
        switch(blobCodec) {
            case BlobCodecRaw: {
                NativeNaturalType blobLength = srcBlob.getSize();
                encodeNatural(blobLength);
                blob.increaseSize(offset, blobLength);
                blob.interoperation(srcBlob, offset, 0, blobLength);
                offset += blobLength;
            } break;
        }
    }

    void encodeEntity(bool doWrite, Symbol entity, Symbol betaSymbol) {
        encodeSymbol(doWrite, entity);
        if(doWrite)
            encodeBlob(entity);
        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = betaSymbol;
        if(doWrite)
            encodeNatural(beta.size());
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            encodeAttribute(doWrite, betaResult.key, betaResult.value);
        });
    }

    void encodeEntities(bool doWrite) {
        for(NativeNaturalType at = symbolIndexOffset; at < symbolIndexOffset+symbolCount; ++at) {
            auto alphaResult = tripleIndex.readElementAt(at);
            encodeEntity(doWrite, alphaResult.key, alphaResult.value[EAV]);
        }
    }

    struct HuffmanParentNode {
        NativeNaturalType parent;
        bool bit;
    };

    struct TreeStackNode {
        NativeNaturalType index;
        Natural8 state;
    };

    void encodeHuffmanTree() {
        if(symbolCount < 2) {
            if(symbolCount == 1)
                encodeNatural(symbolMap.readKeyAt(0)+1);
            return;
        }
        encodeEntities(false);

        NativeNaturalType index = 0,
                          huffmanChildrenCount = symbolCount-1,
                          huffmanParentsCount = huffmanChildrenCount*2;
        BlobHeap<true, NativeNaturalType, Symbol> symbolHeap;
        symbolHeap.reserve(symbolCount);
        symbolMap.iterate([&](Pair<Symbol, NativeNaturalType> pair) {
            symbolHeap.writeElementAt(index, {pair.value, index});
            ++index;
        });
        symbolHeap.build();

        BlobVector<true, HuffmanParentNode> huffmanParents;
        BlobVector<true, NativeNaturalType> huffmanChildren;
        huffmanParents.reserve(huffmanParentsCount);
        huffmanChildren.reserve(huffmanParentsCount);
        for(NativeNaturalType index = 0; symbolHeap.size() > 1; ++index) {
            auto a = symbolHeap.pop_front(),
                 b = symbolHeap.pop_front();
            NativeNaturalType parent = symbolCount+index;
            huffmanParents.writeElementAt(a.value, {parent, 0});
            huffmanParents.writeElementAt(b.value, {parent, 1});
            huffmanChildren.writeElementAt(index*2, a.value);
            huffmanChildren.writeElementAt(index*2+1, b.value);
            symbolHeap.insertElement({a.key+b.key, parent});
        }
        symbolHeap.clear();

        NativeNaturalType huffmanCodesOffset = 0;
        for(NativeNaturalType i = 0; i < symbolCount; ++i) {
            NativeNaturalType index = i, length = 0, reserveOffset = 1;
            while(index < huffmanParentsCount) {
                index = huffmanParents.readElementAt(index).parent;
                ++length;
            }
            symbolMap.writeValueAt(i, huffmanCodesOffset);
            huffmanCodes.increaseSize(huffmanCodesOffset, length);
            huffmanCodesOffset += length;
            index = i;
            while(index < huffmanParentsCount) {
                auto node = huffmanParents.readElementAt(index);
                index = node.parent;
                huffmanCodes.externalOperate<true>(&node.bit, huffmanCodesOffset-reserveOffset, 1);
                ++reserveOffset;
            }
        }
        huffmanParents.clear();

        BlobVector<true, TreeStackNode> stack;
        stack.push_back({huffmanParentsCount, 0});
        while(!stack.empty()) {
            auto element = stack.back();
            switch(element.state) {
                case 0:
                case 1: {
                    NativeNaturalType index = huffmanChildren.readElementAt((element.index-symbolCount)*2+element.state);
                    ++element.state;
                    stack.writeElementAt(stack.size()-1, element);
                    stack.push_back({index, static_cast<Natural8>((index < symbolCount) ? 3 : 0)});
                } break;
                case 2:
                    stack.pop_back();
                    encodeNatural(0);
                    break;
                case 3: {
                    Symbol symbol = symbolMap.readKeyAt(element.index);
                    encodeNatural(symbol+1);
                    stack.pop_back();
                } break;
            }
        }
        huffmanChildren.clear();
    }

    void encodeChunk() {
        encodeNatural(numberCodec);
        encodeNatural(symbolCodec);
        encodeNatural(blobCodec);
        encodeNatural(symbolCount);
        if(symbolCodec == SymbolCodecHuffman)
            encodeHuffmanTree();
        encodeEntities(true);
    }

    void encode() {
        symbolIndexOffset = 0;
        symbolCount = tripleIndex.size();
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

    BinaryEncoder() :BinaryCodec() {
        symbolMap.symbol = BinaryCodecAux0Symbol;
        huffmanCodes = Blob(BinaryCodecAux1Symbol);
        blob.setSize(0);
    }

    ~BinaryEncoder() {
        Blob(symbolMap.symbol).setSize(0);
        Blob(huffmanCodes.symbol).setSize(0);
    }
};

struct BinaryDecoder : public BinaryCodec {
    BlobVector<false, Symbol> symbolVector;
    BlobVector<false, NativeNaturalType> huffmanChildren;

    NativeNaturalType decodeNatural() {
        switch(numberCodec) {
            case NumberCodecRaw: {
                NativeNaturalType value;
                blob.externalOperate<false>(&value, offset, naturalLength);
                offset += naturalLength;
                return value;
            }
            case NumberCodecBinaryVariableLength:
                return blob.decodeBvlNatural(offset);
        }
        assert(false);
        return 0;
    }

    Symbol decodeSymbol() {
        Symbol symbol;
        switch(symbolCodec) {
            case SymbolCodecNatural:
                symbol = decodeNatural();
                break;
            case SymbolCodecHuffman: {
                NativeNaturalType index;
                if(symbolCount < 2)
                    index = 0;
                else {
                    NativeNaturalType mask = 0, bitsLeft = 0;
                    index = symbolCount-2;
                    while(true) {
                        if(bitsLeft == 0) {
                            bitsLeft = min(static_cast<NativeNaturalType>(architectureSize), blob.getSize()-offset);
                            assert(bitsLeft);
                            blob.externalOperate<false>(&mask, offset, bitsLeft);
                            offset += bitsLeft;
                        }
                        index = huffmanChildren.readElementAt(index*2+(mask&1));
                        --bitsLeft;
                        if(index < symbolCount)
                            break;
                        index -= symbolCount;
                        mask >>= 1;
                    }
                    offset -= bitsLeft;
                }
                symbol = symbolVector.readElementAt(index);
            } break;
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

    void decodeBlob(Symbol symbol) {
        Blob dstBlob(symbol);
        switch(blobCodec) {
            case BlobCodecRaw: {
                NativeNaturalType blobLength = decodeNatural();
                dstBlob.setSize(blobLength);
                dstBlob.interoperation(blob, 0, offset, blobLength);
                offset += blobLength;
            } break;
        }
    }

    void decodeEntity() {
        Symbol entity = decodeSymbol();
        decodeBlob(entity);
        NativeNaturalType attributeCount = decodeNatural();
        for(NativeNaturalType i = 0; i < attributeCount; ++i)
            decodeAttribute(entity);
    }

    void decodeHuffmanTree() {
        symbolVector.reserve(symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                symbolVector.writeElementAt(0, decodeNatural()-1);
            return;
        }
        huffmanChildren.reserve((symbolCount-1)*2);
        BlobVector<true, NativeNaturalType> stack;
        NativeNaturalType symbolIndex = 0, huffmanChildrenIndex = 0;
        while(huffmanChildrenIndex < symbolCount-1) {
            Symbol symbol = decodeNatural();
            if(symbol > 0) {
                --symbol;
                if(symbol > preDefinedSymbolsCount) // TODO
                    symbol = createSymbol();
                symbolVector.writeElementAt(symbolIndex, symbol);
                stack.push_back(symbolIndex++);
            } else {
                NativeNaturalType one = stack.pop_back(), zero = stack.pop_back();
                huffmanChildren.writeElementAt(huffmanChildrenIndex*2, zero);
                huffmanChildren.writeElementAt(huffmanChildrenIndex*2+1, one);
                stack.push_back(symbolCount+huffmanChildrenIndex);
                ++huffmanChildrenIndex;
            }
        }
    }

    void decodeChunk() {
        numberCodec = static_cast<NumberCodec>(decodeNatural());
        symbolCodec = static_cast<SymbolCodec>(decodeNatural());
        blobCodec = static_cast<BlobCodec>(decodeNatural());
        symbolCount = decodeNatural();
        if(symbolCodec == SymbolCodecHuffman)
            decodeHuffmanTree();
        for(NativeNaturalType i = 0; i < symbolCount; ++i)
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

    BinaryDecoder() :BinaryCodec() {
        symbolVector.symbol = BinaryCodecAux0Symbol;
        huffmanChildren.symbol = BinaryCodecAux1Symbol;
    }

    ~BinaryDecoder() {
        Blob(symbolVector.symbol).setSize(0);
        Blob(huffmanChildren.symbol).setSize(0);
    }
};
