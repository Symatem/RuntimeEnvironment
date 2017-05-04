#include <Storage/DataStructures.hpp>

template<typename Container>
void setElementCount(Container container, NativeNaturalType newElementCount) {
    NativeNaturalType elementCount = container.getElementCount();
    if(newElementCount > elementCount)
        container.insertRange(elementCount, elementCount-newElementCount);
    else if(newElementCount < elementCount)
        container.eraseRange(newElementCount, newElementCount-elementCount);
}

template<typename Container>
void swapElementsAt(Container container, NativeNaturalType a, NativeNaturalType b) {
    typename Container::ElementType elementA = container.getElementAt(a), elementB = container.getElementAt(b);
    container.setElementAt(a, elementB);
    container.setElementAt(b, elementA);
}

template<typename Container>
void iterateElements(Container container, Closure<void(typename Container::ElementType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(container.getElementAt(at));
}

template<typename Container>
void iterateKeys(Container container, Closure<void(typename Container::ElementType::FirstType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(container.getKeyAt(at));
}

template<typename Container>
void iterateValues(Container container, Closure<void(typename Container::ElementType::SecondType)> callback) {
    for(NativeNaturalType at = 0; at < container.getElementCount(); ++at)
        callback(container.getValueAt(at));
}

template<typename Container>
typename Container::ElementType getFirstElement(Container container) {
    return container.getElementAt(0);
}

template<typename Container>
typename Container::ElementType getLastElement(Container container) {
    return container.getElementAt(container.getElementCount()-1);
}

template<typename Container>
void insertAsFirstElement(Container container, typename Container::ElementType element) {
    container.insertElementAt(0, element);
}

template<typename Container>
void insertAsLastElement(Container container, typename Container::ElementType element) {
    container.insertElementAt(container.getElementCount(), element);
}

template<typename Container>
typename Container::ElementType getAndEraseElementAt(Container container, NativeNaturalType at) {
    typename Container::ElementType element = container.getElementAt(at);
    container.eraseElementAt(at);
    return element;
}

template<typename Container>
typename Container::ElementType eraseFirstElement(Container container) {
    return getAndEraseElementAt(container, 0);
}

template<typename Container>
typename Container::ElementType eraseLastElement(Container container) {
    return getAndEraseElementAt(container, container.getElementCount()-1);
}



struct BitstreamContainer {
    Blob blob;

    BitstreamContainer(Symbol symbol) :blob(Blob(symbol)) {}

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

struct GuardedBitstreamContainer : public BitstreamContainer {
    typedef BitstreamContainer Super;

    GuardedBitstreamContainer() :Super(createSymbol()) {}

    ~GuardedBitstreamContainer() {
        releaseSymbol(Super::blob.symbol);
    }
};

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
    // void setElementAt(NativeNaturalType at, ElementType element) = delete;
    void insertRange(NativeNaturalType at, NativeNaturalType elementCount) = delete;
    void insertElementAt(NativeNaturalType at, ElementType element) = delete;

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
    void setElementAt(NativeNaturalType at, ElementType element) = delete;
    void insertRange(NativeNaturalType at, NativeNaturalType elementCount) = delete;
    void insertElementAt(NativeNaturalType at, ElementType element) = delete;

    bool findKey(KeyType key, NativeNaturalType& at) {
        at = binarySearch<NativeNaturalType>(0, Super::getElementCount(), [&](NativeNaturalType at) {
            return Super::getKeyAt(at) < key;
        });
        return (at < Super::getElementCount() && Super::getKeyAt(at) == key);
    }

    bool insertElement(ElementType element) {
        NativeNaturalType at;
        if(findKey(element.first, at))
            return false;
        Super::insertElementAt(at, element);
        return true;
    }

    bool eraseElement(KeyType key) {
        NativeNaturalType at;
        if(!findKey(key, at))
            return false;
        Super::eraseElementAt(at);
        return true;
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
