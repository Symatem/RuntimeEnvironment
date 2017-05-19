#include <External/BinaryVariableLength.hpp>

struct StaticHuffmanCodec {
    BitVector& bitVector;
    NativeNaturalType& offset;
    NativeNaturalType symbolCount;

    StaticHuffmanCodec(BitVector& _bitVector, NativeNaturalType& _offset) :bitVector(_bitVector), offset(_offset) {}
};

struct StaticHuffmanEncoder : public StaticHuffmanCodec {
    BitVectorGuard<DataStructure<Set<Symbol, NativeNaturalType>>> symbolMap;
    BitVector huffmanCodes;

    void countSymbol(Symbol symbol) {
        NativeNaturalType index;
        if(symbolMap.findKey(symbol, index))
            symbolMap.setValueAt(index, symbolMap.getValueAt(index)+1);
        else
            symbolMap.insertElement(Pair<Symbol, NativeNaturalType>{symbol, 1});
    }

    void encodeSymbol(Symbol symbol) {
        NativeNaturalType index;
        assert(symbolMap.findKey(symbol, index));
        if(symbolCount < 2)
            return;
        NativeNaturalType begin = symbolMap.getValueAt(index),
                          length = (index+1 < symbolCount) ? symbolMap.getValueAt(index+1)-begin : huffmanCodes.getSize()-begin;
        bitVector.increaseSize(offset, length);
        bitVector.interoperation(huffmanCodes, offset, begin, length);
        offset += length;
    }

    void encodeTree() {
        symbolCount = symbolMap.getElementCount();
        encodeBvlNatural(bitVector, offset, symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                encodeBvlNatural(bitVector, offset, symbolMap.getKeyAt(0));
            return;
        }

        NativeNaturalType index = 0,
                          huffmanChildrenCount = symbolCount-1,
                          huffmanParentsCount = huffmanChildrenCount*2;
        BitVectorGuard<DataStructure<Heap<Ascending, NativeNaturalType, Symbol>>> symbolHeap;
        symbolHeap.setElementCount(symbolCount);
        symbolMap.iterateElements([&](Pair<Symbol, NativeNaturalType> pair) {
            symbolHeap.setElementAt(index, {pair.second, index});
            ++index;
        });
        symbolHeap.build();

        struct HuffmanParentNode {
            NativeNaturalType parent;
            bool bit;
        };
        BitVectorGuard<DataStructure<Vector<HuffmanParentNode>>> huffmanParents;
        BitVectorGuard<DataStructure<Vector<NativeNaturalType>>> huffmanChildren;
        huffmanParents.setElementCount(huffmanParentsCount);
        huffmanChildren.setElementCount(huffmanParentsCount);
        for(NativeNaturalType index = 0; symbolHeap.getElementCount() > 1; ++index) {
            auto a = symbolHeap.eraseFirstElement(),
                 b = symbolHeap.eraseFirstElement();
            NativeNaturalType parent = symbolCount+index;
            huffmanParents.setElementAt(a.second, {parent, 0});
            huffmanParents.setElementAt(b.second, {parent, 1});
            huffmanChildren.setElementAt(index*2, a.second);
            huffmanChildren.setElementAt(index*2+1, b.second);
            symbolHeap.insertElement({a.first+b.first, parent});
        }
        symbolHeap.setElementCount(0);

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
        huffmanParents.setElementCount(0);

        struct HuffmanStackElement {
            NativeNaturalType index;
            Natural8 state;
        };
        BitVectorGuard<DataStructure<Vector<HuffmanStackElement>>> stack;
        stack.insertAsLastElement(HuffmanStackElement{huffmanParentsCount, 0});
        while(!stack.isEmpty()) {
            auto element = stack.getLastElement();
            switch(element.state) {
                case 0:
                case 1: {
                    NativeNaturalType index = huffmanChildren.getElementAt((element.index-symbolCount)*2+element.state);
                    ++element.state;
                    stack.setElementAt(stack.getElementCount()-1, element);
                    stack.insertAsLastElement(HuffmanStackElement{index, static_cast<Natural8>((index < symbolCount) ? 3 : 0)});
                } break;
                case 2:
                    stack.eraseLastElement();
                    encodeBvlNatural(bitVector, offset, 0);
                    break;
                case 3: {
                    Symbol symbol = symbolMap.getKeyAt(element.index);
                    encodeBvlNatural(bitVector, offset, symbol+1);
                    stack.eraseLastElement();
                } break;
            }
        }
        huffmanChildren.setElementCount(0);
    }

    StaticHuffmanEncoder(BitVector& _bitVector, NativeNaturalType& _offset) :StaticHuffmanCodec(_bitVector, _offset) {
        huffmanCodes = BitVector(createSymbol());
    }

    ~StaticHuffmanEncoder() {
        releaseSymbol(huffmanCodes.symbol);
    }
};

struct StaticHuffmanDecoder : public StaticHuffmanCodec {
    BitVectorGuard<DataStructure<Vector<Symbol>>> symbolVector;
    BitVectorGuard<DataStructure<Vector<NativeNaturalType>>> huffmanChildren;

    Symbol decodeSymbol() {
        NativeNaturalType index;
        if(symbolCount < 2)
            index = 0;
        else {
            NativeNaturalType mask = 0, bitsLeft = 0;
            index = symbolCount-2;
            while(true) {
                if(bitsLeft == 0) {
                    bitsLeft = min(static_cast<NativeNaturalType>(architectureSize), bitVector.getSize()-offset);
                    assert(bitsLeft);
                    bitVector.externalOperate<false>(&mask, offset, bitsLeft);
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
        symbolCount = decodeBvlNatural(bitVector, offset);
        symbolVector.setElementCount(symbolCount);
        if(symbolCount < 2) {
            if(symbolCount == 1)
                symbolVector.setElementAt(0, decodeBvlNatural(bitVector, offset));
            return;
        }
        huffmanChildren.setElementCount((symbolCount-1)*2);
        BitVectorGuard<DataStructure<Vector<NativeNaturalType>>> stack;
        NativeNaturalType symbolIndex = 0, huffmanChildrenIndex = 0;
        while(huffmanChildrenIndex < symbolCount-1) {
            Symbol symbol = decodeBvlNatural(bitVector, offset);
            if(symbol > 0) {
                --symbol;
                symbolVector.setElementAt(symbolIndex, symbol);
                stack.insertAsLastElement(symbolIndex++);
            } else {
                NativeNaturalType one = stack.eraseLastElement(),
                                 zero = stack.eraseLastElement();
                huffmanChildren.setElementAt(huffmanChildrenIndex*2, zero);
                huffmanChildren.setElementAt(huffmanChildrenIndex*2+1, one);
                stack.insertAsLastElement(symbolCount+huffmanChildrenIndex);
                ++huffmanChildrenIndex;
            }
        }
    }

    StaticHuffmanDecoder(BitVector& _bitVector, NativeNaturalType& _offset) :StaticHuffmanCodec(_bitVector, _offset) {}

    ~StaticHuffmanDecoder() {
        symbolVector.setElementCount(0);
        huffmanChildren.setElementCount(0);
    }
};
