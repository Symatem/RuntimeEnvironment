#include <External/BinaryVariableLength.hpp>

struct StaticHuffmanCodec {
    Blob& blob;
    NativeNaturalType& offset;
    NativeNaturalType symbolCount;

    StaticHuffmanCodec(Blob& _blob, NativeNaturalType& _offset) :blob(_blob), offset(_offset) {}
};

struct StaticHuffmanEncoder : public StaticHuffmanCodec {
    GuardedBitstreamDataStructure<BitstreamSet<Symbol, NativeNaturalType>> symbolMap;
    Blob huffmanCodes;

    void countSymbol(Symbol symbol) {
        NativeNaturalType index;
        if(symbolMap.findKey(symbol, index))
            symbolMap.setValueAt(index, symbolMap.getValueAt(index)+1);
        else
            insertElement(symbolMap, {symbol, 1});
    }

    void encodeSymbol(Symbol symbol) {
        NativeNaturalType index;
        assert(symbolMap.findKey(symbol, index));
        if(symbolCount < 2)
            return;
        NativeNaturalType begin = symbolMap.getValueAt(index),
                          length = (index+1 < symbolCount) ? symbolMap.getValueAt(index+1)-begin : huffmanCodes.getSize()-begin;
        blob.increaseSize(offset, length);
        blob.interoperation(huffmanCodes, offset, begin, length);
        offset += length;
    }

    void encodeTree() {
        symbolCount = symbolMap.getElementCount();
        encodeBvlNatural(blob, offset, symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                encodeBvlNatural(blob, offset, symbolMap.getKeyAt(0));
            return;
        }

        NativeNaturalType index = 0,
                          huffmanChildrenCount = symbolCount-1,
                          huffmanParentsCount = huffmanChildrenCount*2;
        GuardedBitstreamDataStructure<BitstreamHeap<Ascending, NativeNaturalType, Symbol>> symbolHeap;
        setElementCount(symbolHeap, symbolCount);
        iterateElements(symbolMap, [&](Pair<Symbol, NativeNaturalType> pair) {
            symbolHeap.setElementAt(index, {pair.second, index});
            ++index;
        });
        symbolHeap.build();

        struct HuffmanParentNode {
            NativeNaturalType parent;
            bool bit;
        };
        GuardedBitstreamDataStructure<BitstreamVector<HuffmanParentNode>> huffmanParents;
        GuardedBitstreamDataStructure<BitstreamVector<NativeNaturalType>> huffmanChildren;
        setElementCount(huffmanParents, huffmanParentsCount);
        setElementCount(huffmanChildren, huffmanParentsCount);
        for(NativeNaturalType index = 0; symbolHeap.getElementCount() > 1; ++index) {
            auto a = eraseFirstElement(symbolHeap),
                 b = eraseFirstElement(symbolHeap);
            NativeNaturalType parent = symbolCount+index;
            huffmanParents.setElementAt(a.second, {parent, 0});
            huffmanParents.setElementAt(b.second, {parent, 1});
            huffmanChildren.setElementAt(index*2, a.second);
            huffmanChildren.setElementAt(index*2+1, b.second);
            symbolHeap.insertElement({a.first+b.first, parent});
        }
        setElementCount(symbolHeap, 0);

        huffmanCodes.setSize(0);
        NativeNaturalType huffmanCodesOffset = 0;
        for(NativeNaturalType i = 0; i < symbolCount; ++i) {
            NativeNaturalType index = i, length = 0, reserveOffset = 1;
            while(index < huffmanParentsCount) {
                index = huffmanParents.getElementAt(index).parent;
                ++length;
            }
            symbolMap.setValueAt(i, huffmanCodesOffset);
            huffmanCodes.increaseSize(huffmanCodesOffset, length);
            huffmanCodesOffset += length;
            index = i;
            while(index < huffmanParentsCount) {
                auto node = huffmanParents.getElementAt(index);
                index = node.parent;
                huffmanCodes.externalOperate<true>(&node.bit, huffmanCodesOffset-reserveOffset, 1);
                ++reserveOffset;
            }
        }
        setElementCount(huffmanParents, 0);

        struct HuffmanStackElement {
            NativeNaturalType index;
            Natural8 state;
        };
        GuardedBitstreamDataStructure<BitstreamVector<HuffmanStackElement>> stack;
        insertAsLastElement(stack, {huffmanParentsCount, 0});
        while(!stack.isEmpty()) {
            auto element = getLastElement(stack);
            switch(element.state) {
                case 0:
                case 1: {
                    NativeNaturalType index = huffmanChildren.getElementAt((element.index-symbolCount)*2+element.state);
                    ++element.state;
                    stack.setElementAt(stack.getElementCount()-1, element);
                    insertAsLastElement(stack, {index, static_cast<Natural8>((index < symbolCount) ? 3 : 0)});
                } break;
                case 2:
                    eraseLastElement(stack);
                    encodeBvlNatural(blob, offset, 0);
                    break;
                case 3: {
                    Symbol symbol = symbolMap.getKeyAt(element.index);
                    encodeBvlNatural(blob, offset, symbol+1);
                    eraseLastElement(stack);
                } break;
            }
        }
        setElementCount(huffmanChildren, 0);
    }

    StaticHuffmanEncoder(Blob& _blob, NativeNaturalType& _offset) :StaticHuffmanCodec(_blob, _offset) {
        huffmanCodes = Blob(createSymbol());
    }

    ~StaticHuffmanEncoder() {
        releaseSymbol(huffmanCodes.symbol);
    }
};

struct StaticHuffmanDecoder : public StaticHuffmanCodec {
    GuardedBitstreamDataStructure<BitstreamVector<Symbol>> symbolVector;
    GuardedBitstreamDataStructure<BitstreamVector<NativeNaturalType>> huffmanChildren;

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
                index = huffmanChildren.getElementAt(index*2+(mask&1));
                --bitsLeft;
                if(index < symbolCount)
                    break;
                index -= symbolCount;
                mask >>= 1;
            }
            offset -= bitsLeft;
        }
        return symbolVector.getElementAt(index);
    }

    void decodeTree() {
        symbolCount = decodeBvlNatural(blob, offset);
        setElementCount(symbolVector, symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                symbolVector.setElementAt(0, decodeBvlNatural(blob, offset));
            return;
        }
        setElementCount(huffmanChildren, (symbolCount-1)*2);
        GuardedBitstreamDataStructure<BitstreamVector<NativeNaturalType>> stack;
        NativeNaturalType symbolIndex = 0, huffmanChildrenIndex = 0;
        while(huffmanChildrenIndex < symbolCount-1) {
            Symbol symbol = decodeBvlNatural(blob, offset);
            if(symbol > 0) {
                --symbol;
                symbolVector.setElementAt(symbolIndex, symbol);
                insertAsLastElement(stack, symbolIndex++);
            } else {
                NativeNaturalType one = eraseLastElement(stack),
                                 zero = eraseLastElement(stack);
                huffmanChildren.setElementAt(huffmanChildrenIndex*2, zero);
                huffmanChildren.setElementAt(huffmanChildrenIndex*2+1, one);
                insertAsLastElement(stack, symbolCount+huffmanChildrenIndex);
                ++huffmanChildrenIndex;
            }
        }
    }

    StaticHuffmanDecoder(Blob& _blob, NativeNaturalType& _offset) :StaticHuffmanCodec(_blob, _offset) {}

    ~StaticHuffmanDecoder() {
        setElementCount(symbolVector, 0);
        setElementCount(huffmanChildren, 0);
    }
};
