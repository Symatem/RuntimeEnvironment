#include <Ontology/Triple.hpp>

struct BinaryCodec {
    Blob blob;
    NativeNaturalType offset;

    BinaryCodec(Symbol symbol) :blob(Blob(symbol)), offset(0) { }
};

struct BinaryEncoder : public BinaryCodec {
    NativeNaturalType symbolCount;
    BlobSet<true, Symbol, NativeNaturalType> symbolMap;
    Blob huffmanCodes;

    void encodeSymbol(bool doWrite, Symbol symbol) {
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
    }

    void encodeAttribute(bool doWrite, Symbol attribute, Symbol gammaSymbol) {
        encodeSymbol(doWrite, attribute);

        BlobSet<false, Symbol> gamma;
        gamma.symbol = gammaSymbol;
        if(doWrite)
            blob.encodeBvlNatural(offset, gamma.size()-1);
        gamma.iterateKeys([&](Symbol gammaResult) {
            encodeSymbol(doWrite, gammaResult);
        });
    }

    void encodeEntity(bool doWrite, Symbol entity, Symbol betaSymbol) {
        encodeSymbol(doWrite, entity);

        if(doWrite) {
            Blob srcBlob(entity);
            NativeNaturalType blobLength = srcBlob.getSize();
            blob.encodeBvlNatural(offset, blobLength);
            blob.increaseSize(offset, blobLength);
            blob.interoperation(srcBlob, offset, 0, blobLength);
            offset += blobLength;
        }

        BlobSet<false, Symbol, Symbol> beta;
        beta.symbol = betaSymbol;
        if(doWrite)
            blob.encodeBvlNatural(offset, beta.size());
        beta.iterate([&](Pair<Symbol, Symbol> betaResult) {
            encodeAttribute(doWrite, betaResult.key, betaResult.value);
        });
    }

    void encodeEntities(bool doWrite) {
        tripleIndex.iterate([&](Pair<Symbol, Symbol[6]> alphaResult) {
            encodeEntity(doWrite, alphaResult.key, alphaResult.value[EAV]);
        });
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
        symbolCount = symbolMap.size();
        blob.encodeBvlNatural(offset, symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                blob.encodeBvlNatural(offset, symbolMap.readKeyAt(0)+1);
            return;
        }

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
                    blob.encodeBvlNatural(offset, 0);
                break;
                case 3: {
                    Symbol symbol = symbolMap.readKeyAt(element.index);
                    blob.encodeBvlNatural(offset, symbol+1);
                    stack.pop_back();
                } break;
            }
        }
        huffmanChildren.clear();
    }

    void encode() {
        encodeEntities(false);
        encodeHuffmanTree();
        encodeEntities(true);
    }

    BinaryEncoder(Symbol symbol) :BinaryCodec(symbol), huffmanCodes(Blob(createSymbol())) {
        blob.setSize(0);
    }

    ~BinaryEncoder() {
        releaseSymbol(huffmanCodes.symbol);
    }
};

struct BinaryDecoder : public BinaryCodec {
    NativeNaturalType symbolCount;
    BlobVector<true, Symbol> symbolVector;
    BlobVector<true, NativeNaturalType> huffmanChildren;

    Symbol decodeSymbol() {
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
        Symbol symbol = symbolVector.readElementAt(index);
        if(superPage->symbolsEnd < symbol+1)
            superPage->symbolsEnd = symbol+1;
        return symbol;
    }

    void decodeAttribute(Symbol entity) {
        Symbol attribute = decodeSymbol();

        NativeNaturalType valueCount = blob.decodeBvlNatural(offset)+1;
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

    void decodeHuffmanTree() {
        symbolCount = blob.decodeBvlNatural(offset);
        symbolVector.reserve(symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                symbolVector.writeElementAt(0, blob.decodeBvlNatural(offset)-1);
            return;
        }

        huffmanChildren.reserve((symbolCount-1)*2);
        BlobVector<true, NativeNaturalType> stack;
        NativeNaturalType symbolIndex = 0, huffmanChildrenIndex = 0;
        while(huffmanChildrenIndex < symbolCount-1) {
            Symbol symbol = blob.decodeBvlNatural(offset);
            if(symbol > 0) {
                --symbol;
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

    void decode() {
        decodeHuffmanTree();

        superPage->symbolsEnd = 0;
        NativeNaturalType length = blob.getSize();
        while(offset < length)
            decodeEntity();
    }

    BinaryDecoder(Symbol symbol) :BinaryCodec(symbol) { }
};
