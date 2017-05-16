#include <Storage/Blob.hpp>

struct BitVectorContainer {
    Blob bitVector;

    BitVectorContainer() {}
    BitVectorContainer(Symbol symbol) :bitVector(symbol) {}

    void increaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        assert(at == 0);
        bitVector.increaseSize(offset, length);
    }

    void decreaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        assert(at == 0);
        bitVector.decreaseSize(offset, length);
    }

    NativeNaturalType getChildOffset(NativeNaturalType at) {
        assert(at == 0);
        return 0;
    }

    NativeNaturalType getChildLength(NativeNaturalType at) {
        return bitVector.getSize();
    }

    Blob& getBitVector() {
        return bitVector;
    }
};

template<typename _Super>
struct DataStructure : public _Super {
    typedef _Super Super;
    BitVectorContainer parent;

    DataStructure() :Super(parent) {}
    DataStructure(Symbol symbol) :Super(parent), parent(symbol) {}
};

template<typename _Super>
struct GuardedDataStructure : public DataStructure<_Super> {
    typedef DataStructure<_Super> Super;

    GuardedDataStructure() :Super(createSymbol()) {}

    ~GuardedDataStructure() {
        releaseSymbol(Super::getBitVector().symbol);
    }
};



template<typename Container>
void setElementCount(Container& container, NativeNaturalType newElementCount) {
    NativeNaturalType elementCount = container.getElementCount();
    if(newElementCount > elementCount)
        container.insertRange(elementCount, newElementCount-elementCount);
    else if(newElementCount < elementCount)
        container.eraseRange(newElementCount, elementCount-newElementCount);
}

template<typename Container>
void swapElementsAt(Container& container, NativeNaturalType a, NativeNaturalType b) {
    typename Container::ElementType elementA = container.getElementAt(a), elementB = container.getElementAt(b);
    container.setElementAt(a, elementB);
    container.setElementAt(b, elementA);
}

template<typename Container>
void iterateElements(Container& container, Closure<void(typename Container::ElementType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(container.getElementAt(at));
}

template<typename Container>
void iterateKeys(Container& container, Closure<void(typename Container::ElementType::FirstType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(container.getKeyAt(at));
}

template<typename Container>
void iterateValues(Container& container, Closure<void(typename Container::ElementType::SecondType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(container.getValueAt(at));
}

template<typename Container>
typename Container::ElementType getFirstElement(Container& container) {
    return container.getElementAt(0);
}

template<typename Container>
typename Container::ElementType getLastElement(Container& container) {
    return container.getElementAt(container.getElementCount()-1);
}

template<typename Container>
void insertAsFirstElement(Container& container, typename Container::ElementType element) {
    container.insertElementAt(0, element);
}

template<typename Container>
void insertAsLastElement(Container& container, typename Container::ElementType element) {
    container.insertElementAt(container.getElementCount(), element);
}

template<typename Container>
typename Container::ElementType getAndEraseElementAt(Container& container, NativeNaturalType at) {
    typename Container::ElementType element = container.getElementAt(at);
    container.eraseElementAt(at);
    return element;
}

template<typename Container>
typename Container::ElementType eraseFirstElement(Container& container) {
    return getAndEraseElementAt(container, 0);
}

template<typename Container>
typename Container::ElementType eraseLastElement(Container& container) {
    return getAndEraseElementAt(container, container.getElementCount()-1);
}

template<typename Container>
bool insertElement(Container& container, typename Container::ElementType element) {
    NativeNaturalType at;
    if(container.findKey(element.first, at))
        return false;
    container.insertElementAt(at, element);
    return true;
}

template<typename Container>
bool eraseElement(Container& container, typename Container::ElementType::FirstType key) {
    NativeNaturalType at;
    if(!container.findKey(key, at))
        return false;
    container.eraseElementAt(at);
    return true;
}



template<typename _ElementType, typename _ParentType = BitVectorContainer>
struct Vector {
    typedef _ElementType ElementType;
    typedef _ParentType ParentType;
    ParentType& parent;
    NativeNaturalType childIndex;

    Vector(ParentType& _parent, NativeNaturalType _childIndex = 0) :parent(_parent), childIndex(_childIndex) { }

    Blob& getBitVector() {
        return parent.getBitVector();
    }

    bool operator==(Vector<ElementType, ParentType>& other) {
        return childIndex == other.childIndex && getBitVector() == other.getBitVector();
    }

    NativeNaturalType getElementCount() {
        return parent.getChildLength(childIndex)/sizeOfInBits<ElementType>::value;
    }

    bool isEmpty() {
        return parent.getChildLength(childIndex) == 0;
    }

    NativeNaturalType getOffsetOfElement(NativeNaturalType at) {
        return parent.getChildOffset(childIndex)+at*sizeOfInBits<ElementType>::value;
    }

    void setElementAt(NativeNaturalType at, ElementType element) {
        getBitVector().template externalOperate<true>(&element, getOffsetOfElement(at), sizeOfInBits<ElementType>::value);
    }

    ElementType getElementAt(NativeNaturalType at) {
        ElementType element;
        getBitVector().template externalOperate<false>(&element, getOffsetOfElement(at), sizeOfInBits<ElementType>::value);
        return element;
    }

    void insertRange(NativeNaturalType at, NativeNaturalType elementCount) {
        parent.increaseChildLength(childIndex, getOffsetOfElement(at), elementCount*sizeOfInBits<ElementType>::value);
    }

    void eraseRange(NativeNaturalType at, NativeNaturalType elementCount) {
        parent.decreaseChildLength(childIndex, getOffsetOfElement(at), elementCount*sizeOfInBits<ElementType>::value);
    }

    void insertElementAt(NativeNaturalType at, ElementType element) {
        insertRange(at, 1);
        setElementAt(at, element);
    }

    void eraseElementAt(NativeNaturalType at) {
        eraseRange(at, 1);
    }

    bool moveElementAt(NativeNaturalType dstAt, NativeNaturalType srcAt) {
        if(dstAt == srcAt)
            return false;
        bool upward = (dstAt > srcAt);
        if(upward)
            ++dstAt;
        insertElementAt(dstAt, getElementAt(srcAt));
        if(!upward)
            ++srcAt;
        eraseElementAt(srcAt);
        return true;
    }
};

template<typename KeyType, typename ValueType, typename _ParentType = BitVectorContainer>
struct PairVector : public Vector<Pair<KeyType, ValueType>, _ParentType> {
    typedef _ParentType ParentType;
    typedef Pair<KeyType, ValueType> ElementType;
    typedef Vector<ElementType, ParentType> Super;

    PairVector(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    NativeNaturalType getOffsetOfKey(NativeNaturalType at) {
        return Super::getOffsetOfElement(at);
    }

    NativeNaturalType getOffsetOfValue(NativeNaturalType at) {
        return Super::getOffsetOfElement(at)+sizeOfInBits<KeyType>::value;
    }

    bool setKeyAt(NativeNaturalType at, KeyType key) {
        Super::getBitVector().template externalOperate<true>(&key, getOffsetOfKey(at), sizeOfInBits<KeyType>::value);
        return true;
    }

    bool setValueAt(NativeNaturalType at, ValueType value) {
        Super::getBitVector().template externalOperate<true>(&value, getOffsetOfValue(at), sizeOfInBits<ValueType>::value);
        return true;
    }

    KeyType getKeyAt(NativeNaturalType at) {
        KeyType key;
        Super::getBitVector().template externalOperate<false>(&key, getOffsetOfKey(at), sizeOfInBits<KeyType>::value);
        return key;
    }

    ValueType getValueAt(NativeNaturalType at) {
        ValueType value;
        Super::getBitVector().template externalOperate<false>(&value, getOffsetOfValue(at), sizeOfInBits<ValueType>::value);
        return value;
    }
};

template<typename SortDirection, typename KeyType, typename ValueType = VoidType, typename _ParentType = BitVectorContainer>
struct Heap : public PairVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef PairVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    Heap(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    bool compare(NativeNaturalType atA, NativeNaturalType atB) {
        return SortDirection::compare(Super::getKeyAt(atA), Super::getKeyAt(atB));
    }

    void siftToRoot(NativeNaturalType at) {
        while(at > 0) {
            NativeNaturalType parent = (at-1)/2;
            if(!compare(at, parent))
                break;
            swapElementsAt(*this, at, parent);
            at = parent;
        }
    }

    void siftToLeaves(NativeNaturalType at, NativeNaturalType elementCount) {
        while(true) {
            NativeNaturalType left = 2*at+1,
                              right = 2*at+2,
                              min = at;
            if(left < elementCount && compare(left, min))
                min = left;
            if(right < elementCount && compare(right, min))
                min = right;
            if(min == at)
                break;
            swapElementsAt(*this, at, min);
            at = min;
        }
    }

    void build() {
        NativeNaturalType elementCount = Super::getElementCount();
        for(NativeIntegerType i = elementCount/2-1; i >= 0; --i)
            siftToLeaves(i, elementCount);
    }

    void reverseSort() {
        build();
        NativeNaturalType elementCount = Super::getElementCount();
        while(elementCount > 1) {
            --elementCount;
            swapElementsAt(*this, 0, elementCount);
            siftToLeaves(0, elementCount);
        }
    }

    void eraseElementAt(NativeNaturalType at) {
        NativeNaturalType elementCount = Super::getElementCount(),
                          parent = (at-1)/2,
                          last = elementCount-1;
        if(at != last)
            Super::setElementAt(at, Super::getElementAt(last));
        Super::eraseElementAt(last);
        if(at != last) {
            if(at == 0 || compare(parent, at))
                siftToLeaves(at, elementCount);
            else
                siftToRoot(at);
        }
    }

    void insertElement(ElementType element) {
        NativeNaturalType at = Super::getElementCount();
        Super::insertElementAt(at, element);
        siftToRoot(at);
    }

    bool setKeyAt(NativeNaturalType at, KeyType key) {
        Super::setKeyAt(at, key);
        siftToRoot(at);
        return true;
    }
};

template<typename KeyType, typename ValueType = VoidType, typename _ParentType = BitVectorContainer>
struct Set : public PairVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef PairVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    Set(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) {}

    bool findKey(KeyType key, NativeNaturalType& at) {
        at = binarySearch<NativeNaturalType>(0, Super::getElementCount(), [&](NativeNaturalType at) {
            return Super::getKeyAt(at) < key;
        });
        return (at < Super::getElementCount() && Super::getKeyAt(at) == key);
    }

    bool setKeyAt(NativeNaturalType at, KeyType key) {
        NativeNaturalType newAt;
        if(findKey(key, newAt))
            return false;
        if(newAt > at)
            --newAt;
        Super::moveElementAt(newAt, at);
        Super::setKeyAt(newAt, key);
        return true;
    }
};



template<typename KeyType, typename ValueType, typename _ParentType = BitVectorContainer>
struct MetaVector : public PairVector<KeyType, NativeNaturalType, _ParentType> {
    typedef _ParentType ParentType;
    typedef PairVector<KeyType, NativeNaturalType, ParentType> Super;
    typedef typename Super::ElementType InnerElementType;
    typedef Pair<KeyType, ValueType> ElementType;

    MetaVector(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    NativeNaturalType getElementCount() {
        return (Super::isEmpty()) ? 0 : Super::getValueAt(0)/sizeOfInBits<InnerElementType>::value;
    }

    ValueType getValueAt(NativeNaturalType at) {
        return ValueType(reinterpret_cast<typename ValueType::ParentType&>(*this), at);
    }

    ElementType getElementAt(NativeNaturalType at) {
        return {Super::getKeyAt(at), getValueAt(at)};
    }

    void insertRange(NativeNaturalType at, NativeNaturalType elementCount) {
        NativeNaturalType newElementCount = getElementCount()+elementCount,
                          value = getChildBegin(at);
        Super::insertRange(at, elementCount);
        for(NativeNaturalType atEnd = at+elementCount; at < atEnd; ++at)
            Super::setValueAt(at, value);
        for(NativeNaturalType at = 0; at < newElementCount; ++at)
            Super::setValueAt(at, Super::getValueAt(at)+elementCount*sizeOfInBits<InnerElementType>::value);
    }

    template<bool childrenAsWell = true>
    void eraseRange(NativeNaturalType at, NativeNaturalType elementCount) {
        NativeNaturalType newElementCount = getElementCount()-elementCount,
                          sliceLength = getChildEnd(at+elementCount-1)-getChildBegin(at);
        if(childrenAsWell)
            Super::parent.decreaseChildLength(Super::childIndex, getChildOffset(at), sliceLength);
        Super::eraseRange(at, elementCount);
        for(NativeNaturalType at = 0; at < newElementCount; ++at)
            Super::setValueAt(at, Super::getValueAt(at)-elementCount*sizeOfInBits<InnerElementType>::value);
        if(childrenAsWell)
            for(; at < newElementCount; ++at)
                Super::setValueAt(at, Super::getValueAt(at)-sliceLength);
    }

    void insertElementAt(NativeNaturalType at, KeyType key) {
        insertRange(at, 1);
        Super::setKeyAt(at, key);
    }

    void eraseElementAt(NativeNaturalType at) {
        eraseRange(at, 1);
    }

    bool moveElementAt(NativeNaturalType dstAt, NativeNaturalType srcAt) {
        if(dstAt == srcAt)
            return false;
        bool upward = (dstAt > srcAt);
        if(upward)
            ++dstAt;
        NativeNaturalType srcOffset = getChildOffset(srcAt), length = getChildLength(srcAt);
        insertElementAt(dstAt, Super::getKeyAt(srcAt));
        NativeNaturalType dstOffset = getChildOffset(dstAt);
        increaseChildLength(dstAt, dstOffset, length);
        Super::getBitVector().interoperation(Super::getBitVector(), dstOffset, srcOffset, length);
        if(!upward)
            ++srcAt;
        eraseElementAt(srcAt);
        return true;
    }



    void increaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        Super::parent.increaseChildLength(Super::childIndex, offset, length);
        for(++at; at < getElementCount(); ++at)
            Super::setValueAt(at, Super::getValueAt(at)+length);
    }

    void decreaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        Super::parent.decreaseChildLength(Super::childIndex, offset, length);
        for(++at; at < getElementCount(); ++at)
            Super::setValueAt(at, Super::getValueAt(at)-length);
    }

    NativeNaturalType getChildOffset(NativeNaturalType at) {
        return Super::parent.getChildOffset(Super::childIndex)+getChildBegin(at);
    }

    NativeNaturalType getChildBegin(NativeNaturalType at) {
        if(at == getElementCount())
            return Super::parent.getChildLength(Super::childIndex);
        return Super::getValueAt(at);
    }

    NativeNaturalType getChildEnd(NativeNaturalType at) {
        if(at+1 == getElementCount())
            return Super::parent.getChildLength(Super::childIndex);
        return Super::getValueAt(at+1);
    }

    NativeNaturalType getChildLength(NativeNaturalType at) {
        return getChildEnd(at)-getChildBegin(at);
    }
};

template<typename KeyType, typename ValueType, typename _ParentType = BitVectorContainer>
struct MetaSet : public MetaVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef MetaVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    MetaSet(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    bool findKey(KeyType key, NativeNaturalType& at) {
        at = binarySearch<NativeNaturalType>(0, Super::getElementCount(), [&](NativeNaturalType at) {
            return Super::getKeyAt(at) < key;
        });
        return (at < Super::getElementCount() && Super::getKeyAt(at) == key);
    }

    bool setKeyAt(NativeNaturalType at, KeyType key) {
        NativeNaturalType newAt;
        if(findKey(key, newAt))
            return false;
        if(newAt > at)
            --newAt;
        Super::moveElementAt(newAt, at);
        Super::setKeyAt(newAt, key);
        return true;
    }
};

template<typename FirstKeyType, typename SecondKeyType, typename _ParentType = BitVectorContainer>
struct PairSet : public MetaSet<FirstKeyType, Set<SecondKeyType, VoidType, PairSet<FirstKeyType, SecondKeyType, _ParentType>>, _ParentType> {
    typedef _ParentType ParentType;
    typedef Set<SecondKeyType, VoidType, PairSet<FirstKeyType, SecondKeyType, ParentType>> ValueType;
    typedef MetaSet<FirstKeyType, ValueType, ParentType> Super;
    typedef Pair<FirstKeyType, SecondKeyType> ElementType;

    PairSet(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    FirstKeyType getFirstKeyCount() {
        return Super::getElementCount();
    }

    SecondKeyType getSecondKeyCount(NativeNaturalType firstAt) {
        return Super::getValueAt(firstAt).getElementCount();
    }

    void iterateFirstKeys(Closure<void(FirstKeyType)> callback) {
        iterateKeys(*this, callback);
    }

    void iterateSecondKeys(NativeNaturalType firstAt, Closure<void(SecondKeyType)> callback) {
        auto innerSet = Super::getValueAt(firstAt);
        iterateKeys(innerSet, callback);
    }

    void iterateElements(Closure<void(ElementType)> callback) {
        for(NativeNaturalType firstAt = 0; firstAt < getFirstKeyCount(); ++firstAt) {
            FirstKeyType firstKey = Super::getKeyAt(firstAt);
            auto innerSet = Super::getValueAt(firstAt);
            iterateKeys(innerSet, [&](SecondKeyType secondKey) {
                callback({firstKey, secondKey});
            });
        }
    }

    bool findFirstKey(FirstKeyType firstKey, NativeNaturalType& firstAt) {
        return Super::findKey(firstKey, firstAt);
    }

    bool findSecondKey(SecondKeyType secondKey, NativeNaturalType firstAt, NativeNaturalType& secondAt) {
        return Super::getValueAt(firstAt).findKey(secondKey, secondAt);
    }

    bool findElement(ElementType element, NativeNaturalType& firstAt, NativeNaturalType& secondAt) {
        return findFirstKey(element.first, firstAt) && findSecondKey(element.second, firstAt, secondAt);
    }

    bool insertElement(ElementType element) {
        NativeNaturalType firstAt;
        if(!findFirstKey(element.first, firstAt))
            Super::insertElementAt(firstAt, element.first);
        ValueType innerSet = Super::getValueAt(firstAt);
        return ::insertElement(innerSet, element.second);
    }

    bool eraseElement(ElementType element) {
        NativeNaturalType firstAt;
        if(!findFirstKey(element.first, firstAt))
            return false;
        ValueType innerSet = Super::getValueAt(firstAt);
        if(!::eraseElement(innerSet, element.second))
            return false;
        if(innerSet.getElementCount() == 0)
            Super::eraseElementAt(firstAt);
        return true;
    }
};

template<typename _ParentType = BitVectorContainer>
struct BitMap : public MetaSet<NativeNaturalType, VoidType, _ParentType> {
    typedef _ParentType ParentType;
    typedef MetaSet<NativeNaturalType, VoidType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    BitMap(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    NativeNaturalType getSliceBeginAddress(NativeNaturalType sliceIndex) {
        return Super::getKeyAt(sliceIndex);
    }

    NativeNaturalType getSliceEndAddress(NativeNaturalType sliceIndex) {
        return getSliceBeginAddress(sliceIndex)+Super::getChildLength(sliceIndex);
    }

    template<bool includeFront = false>
    bool getSliceContaining(NativeNaturalType address, NativeNaturalType& sliceIndex) {
        if(Super::findKey(address, sliceIndex) && includeFront)
            return true;
        if(!Super::isEmpty() && sliceIndex > 0 && address < getSliceEndAddress(sliceIndex-1)) {
            --sliceIndex;
            return true;
        }
        return false;
    }

    bool getSliceContaining(NativeNaturalType address, NativeNaturalType length, NativeNaturalType& sliceIndex) {
        if(!getSliceContaining<false>(address, sliceIndex))
            return false;
        return getSliceEndAddress(sliceIndex) <= address+length;
    }

    bool mergeSlices(NativeNaturalType sliceIndex) {
        if(getSliceBeginAddress(sliceIndex+1)-getSliceBeginAddress(sliceIndex) != Super::getChildLength(sliceIndex))
            return false;
        Super::template eraseRange<false>(sliceIndex+1, 1);
        return true;
    }

    NativeNaturalType fillSlice(NativeNaturalType address, NativeNaturalType length) {
        NativeNaturalType endAddress = address+length, sliceIndex,
                          frontSliceIndex, sliceOffset, sliceLength, sliceAddress;
        bool frontSlice = getSliceContaining(address, sliceIndex), backSlice = false;
        if(frontSlice) {
            sliceOffset = address-getSliceBeginAddress(sliceIndex);
            sliceLength = Super::getChildLength(sliceIndex)-sliceOffset;
            sliceOffset += Super::getChildBegin(sliceIndex);
            if(length <= sliceLength)
                return sliceOffset;
            frontSliceIndex = sliceIndex++;
        }
        while(sliceIndex < Super::getElementCount()) {
            if(endAddress < getSliceEndAddress(sliceIndex)) {
                sliceAddress = getSliceBeginAddress(sliceIndex);
                backSlice = (endAddress > sliceAddress);
                break;
            }
            Super::eraseElementAt(sliceIndex);
            // TODO: Optimize, recyle this slice if !frontSlice
        }
        if(frontSlice) {
            length -= sliceLength;
            if(backSlice)
                length -= endAddress-sliceAddress;
            Super::increaseChildLength(frontSliceIndex, sliceOffset+sliceLength, length);
            if(backSlice)
                Super::template eraseRange<false>(sliceIndex, 1);
        } else {
            if(backSlice) {
                length = endAddress-sliceAddress;
                Super::setKeyAt(sliceIndex, address);
            } else
                Super::insertElementAt(sliceIndex, address);
            sliceOffset = Super::getChildBegin(sliceIndex);
            Super::increaseChildLength(sliceIndex, sliceOffset, length);
        }
        if(!backSlice && sliceIndex+1 < Super::getElementCount())
            mergeSlices(sliceIndex);
        if(!frontSlice && sliceIndex > 0)
            mergeSlices(sliceIndex-1);
        return sliceOffset;
    }

    void clearSlice(NativeNaturalType address, NativeNaturalType length) {
        NativeNaturalType endAddress = address+length, sliceIndex;
        if(getSliceContaining(address, sliceIndex)) {
            NativeNaturalType sliceOffset = address-getSliceBeginAddress(sliceIndex),
                              sliceLength = Super::getChildLength(sliceIndex)-sliceOffset;
            bool splitFrontSlice = (sliceLength > length);
            if(splitFrontSlice) {
                sliceLength = length;
                Super::insertElementAt(sliceIndex+1, endAddress);
            }
            sliceOffset += Super::getChildBegin(sliceIndex);
            Super::decreaseChildLength(sliceIndex, sliceOffset, sliceLength);
            if(splitFrontSlice) {
                Super::setValueAt(sliceIndex+1, sliceOffset);
                return;
            }
            ++sliceIndex;
        }
        while(sliceIndex < Super::getElementCount()) {
            if(endAddress < getSliceEndAddress(sliceIndex)) {
                NativeNaturalType sliceAddress = getSliceBeginAddress(sliceIndex);
                if(endAddress > sliceAddress) {
                    Super::decreaseChildLength(sliceIndex, Super::getChildBegin(sliceIndex), endAddress-sliceAddress);
                    Super::setKeyAt(sliceIndex, endAddress);
                }
                break;
            }
            Super::eraseElementAt(sliceIndex);
        }
    }

    bool moveSlice(NativeNaturalType dstAddress, NativeNaturalType srcAddress, NativeNaturalType length) {
        bool downward = (dstAddress < srcAddress);
        NativeNaturalType sliceIndex, eraseLength = length, eraseAddress = dstAddress, addressDiff = downward ? srcAddress-dstAddress : dstAddress-srcAddress;
        if(length < addressDiff) {
            NativeNaturalType beginAddress = downward ? dstAddress : srcAddress,
                              endAddress = beginAddress+addressDiff;
            beginAddress += length;
            if(getSliceContaining<true>(beginAddress, sliceIndex) || (sliceIndex < Super::getElementCount() && getSliceBeginAddress(sliceIndex) < endAddress))
                return false;
        } else if(length > addressDiff) {
            eraseLength = addressDiff;
            if(!downward)
                eraseAddress = dstAddress+addressDiff;
        }
        clearSlice(eraseAddress, eraseLength);
        if(downward) {
            Super::findKey(srcAddress, sliceIndex);
            if(getSliceEndAddress(sliceIndex) > srcAddress+length) {
                NativeNaturalType spareLength = getSliceEndAddress(sliceIndex)-(srcAddress+length);
                Super::insertElementAt(sliceIndex+1, getSliceBeginAddress(sliceIndex)+spareLength);
                Super::setValueAt(sliceIndex+1, Super::getChildBegin(sliceIndex)+spareLength);
            }
        } else {
            if(getSliceContaining<true>(srcAddress, sliceIndex) && getSliceBeginAddress(sliceIndex) < srcAddress) {
                NativeNaturalType spareLength = srcAddress-getSliceBeginAddress(sliceIndex);
                Super::insertElementAt(sliceIndex+1, getSliceEndAddress(sliceIndex)-spareLength);
                Super::setValueAt(sliceIndex+1, Super::getChildEnd(sliceIndex)-spareLength);
                ++sliceIndex;
            }
        }
        NativeNaturalType lowestSliceIndex = sliceIndex;
        while(sliceIndex < Super::getElementCount()) {
            NativeNaturalType sliceAddress = getSliceBeginAddress(sliceIndex);
            if(sliceAddress >= srcAddress+length)
                break;
            Super::setKeyAt(sliceIndex++, downward ? sliceAddress-addressDiff : sliceAddress+addressDiff);
        }
        if(downward) {
            if(lowestSliceIndex > 0)
                mergeSlices(lowestSliceIndex-1);
        } else {
            if(sliceIndex < Super::getElementCount())
                mergeSlices(sliceIndex-1);
        }
        return true;
    }
};
