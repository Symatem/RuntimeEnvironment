#include <Storage/Blob.hpp>

struct BitstreamContainer {
    Blob blob;

    BitstreamContainer() {}
    BitstreamContainer(Symbol symbol) :blob(symbol) {}

    void increaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        assert(at == 0);
        blob.increaseSize(offset, length);
    }

    void decreaseChildLength(NativeNaturalType at, NativeNaturalType offset, NativeNaturalType length) {
        assert(at == 0);
        blob.decreaseSize(offset, length);
    }

    NativeNaturalType getChildOffset(NativeNaturalType at) {
        assert(at == 0);
        return 0;
    }

    NativeNaturalType getChildLength(NativeNaturalType at) {
        return blob.getSize();
    }
};

template<typename _Super>
struct BitstreamDataStructure : public _Super {
    typedef _Super Super;
    BitstreamContainer parent;

    BitstreamDataStructure() :Super(parent) {}
    BitstreamDataStructure(Symbol symbol) :Super(parent), parent(symbol) {}
};

template<typename _Super>
struct GuardedBitstreamDataStructure : public BitstreamDataStructure<_Super> {
    typedef BitstreamDataStructure<_Super> Super;

    GuardedBitstreamDataStructure() :Super(createSymbol()) {}

    ~GuardedBitstreamDataStructure() {
        releaseSymbol(Super::parent.blob.symbol);
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



template<typename _ElementType, typename _ParentType = BitstreamContainer>
struct BitstreamVector {
    typedef _ElementType ElementType;
    typedef _ParentType ParentType;
    ParentType& parent;
    Blob& blob;
    NativeNaturalType childIndex;

    BitstreamVector(ParentType& _parent, NativeNaturalType _childIndex = 0) :parent(_parent), blob(_parent.blob), childIndex(_childIndex) { }

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
        blob.externalOperate<true>(&element, getOffsetOfElement(at), sizeOfInBits<ElementType>::value);
    }

    ElementType getElementAt(NativeNaturalType at) {
        ElementType element;
        blob.externalOperate<false>(&element, getOffsetOfElement(at), sizeOfInBits<ElementType>::value);
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

template<typename KeyType, typename ValueType, typename _ParentType = BitstreamContainer>
struct BitstreamPairVector : public BitstreamVector<Pair<KeyType, ValueType>, _ParentType> {
    typedef _ParentType ParentType;
    typedef Pair<KeyType, ValueType> ElementType;
    typedef BitstreamVector<ElementType, ParentType> Super;

    BitstreamPairVector(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    NativeNaturalType getOffsetOfKey(NativeNaturalType at) {
        return Super::getOffsetOfElement(at);
    }

    NativeNaturalType getOffsetOfValue(NativeNaturalType at) {
        return Super::getOffsetOfElement(at)+sizeOfInBits<KeyType>::value;
    }

    bool setKeyAt(NativeNaturalType at, KeyType key) {
        Super::blob.template externalOperate<true>(&key, getOffsetOfKey(at), sizeOfInBits<KeyType>::value);
        return true;
    }

    bool setValueAt(NativeNaturalType at, ValueType value) {
        Super::blob.template externalOperate<true>(&value, getOffsetOfValue(at), sizeOfInBits<ValueType>::value);
        return true;
    }

    KeyType getKeyAt(NativeNaturalType at) {
        KeyType key;
        Super::blob.template externalOperate<false>(&key, getOffsetOfKey(at), sizeOfInBits<KeyType>::value);
        return key;
    }

    ValueType getValueAt(NativeNaturalType at) {
        ValueType value;
        Super::blob.template externalOperate<false>(&value, getOffsetOfValue(at), sizeOfInBits<ValueType>::value);
        return value;
    }
};

template<typename SortDirection, typename KeyType, typename ValueType = VoidType, typename _ParentType = BitstreamContainer>
struct BitstreamHeap : public BitstreamPairVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef BitstreamPairVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    BitstreamHeap(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

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

template<typename KeyType, typename ValueType = VoidType, typename _ParentType = BitstreamContainer>
struct BitstreamSet : public BitstreamPairVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef BitstreamPairVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    BitstreamSet(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) {}

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



template<typename KeyType, typename ValueType, typename _ParentType = BitstreamContainer>
struct BitstreamContainerVector : public BitstreamPairVector<KeyType, NativeNaturalType, _ParentType> {
    typedef _ParentType ParentType;
    typedef BitstreamPairVector<KeyType, NativeNaturalType, ParentType> Super;
    typedef typename Super::ElementType InnerElementType;
    typedef Pair<KeyType, ValueType> ElementType;

    BitstreamContainerVector(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

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

    void eraseRange(NativeNaturalType at, NativeNaturalType elementCount) {
        NativeNaturalType newElementCount = getElementCount()-elementCount,
                          sliceLength = getChildEnd(at+elementCount-1)-getChildBegin(at);
        Super::parent.decreaseChildLength(Super::childIndex, getChildOffset(at), sliceLength);
        Super::eraseRange(at, elementCount);
        for(; at < newElementCount; ++at)
            Super::setValueAt(at, Super::getValueAt(at)-sliceLength);
        for(NativeNaturalType at = 0; at < newElementCount; ++at)
            Super::setValueAt(at, Super::getValueAt(at)-elementCount*sizeOfInBits<InnerElementType>::value);
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
        Super::blob.interoperation(Super::blob, dstOffset, srcOffset, length);
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
        if(at == getElementCount()-1)
            return Super::parent.getChildLength(Super::childIndex);
        return Super::getValueAt(at+1);
    }

    NativeNaturalType getChildLength(NativeNaturalType at) {
        return getChildEnd(at)-getChildBegin(at);
    }
};

template<typename KeyType, typename ValueType, typename _ParentType = BitstreamContainer>
struct BitstreamContainerSet : public BitstreamContainerVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef BitstreamContainerVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    BitstreamContainerSet(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

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

template<typename FirstKeyType, typename SecondKeyType, typename _ParentType = BitstreamContainer>
struct BitstreamPairSet : public BitstreamContainerSet<FirstKeyType, BitstreamSet<SecondKeyType, VoidType, BitstreamPairSet<FirstKeyType, SecondKeyType, _ParentType>>, _ParentType> {
    typedef _ParentType ParentType;
    typedef BitstreamSet<SecondKeyType, VoidType, BitstreamPairSet<FirstKeyType, SecondKeyType, ParentType>> ValueType;
    typedef BitstreamContainerSet<FirstKeyType, ValueType, ParentType> Super;
    typedef Pair<FirstKeyType, SecondKeyType> ElementType;

    BitstreamPairSet(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

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

template<typename _ParentType = BitstreamContainer>
struct BitstreamSparsePayload : public BitstreamContainerSet<NativeNaturalType, VoidType, _ParentType> {
    typedef _ParentType ParentType;
    typedef BitstreamContainerSet<NativeNaturalType, VoidType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    BitstreamSparsePayload(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }
};
