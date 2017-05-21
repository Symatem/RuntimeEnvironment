#include <DataStructures/Set.hpp>

template<typename KeyType, typename _ParentType = BitVectorContainer>
struct MetaVector : public PairVector<KeyType, NativeNaturalType, _ParentType> {
    typedef _ParentType ParentType;
    typedef PairVector<KeyType, NativeNaturalType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    MetaVector(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }
    usingRemappedMethod(iterateElements)
    usingRemappedMethod(iterate)
    usingRemappedMethod(getLastElement)
    usingRemappedMethod(insertAsLastElement)
    usingRemappedMethod(eraseLastElement)

    NativeNaturalType getElementCount() {
        return (Super::isEmpty()) ? 0 : Super::getValueAt(0)/sizeOfInBits<ElementType>::value;
    }

    template<typename ValueType>
    ValueType getValueAt(NativeNaturalType at) {
        return ValueType(reinterpret_cast<typename ValueType::ParentType&>(*this), at);
    }

    template<typename ValueType>
    Pair<KeyType, ValueType> getElementAt(NativeNaturalType at) {
        return {Super::getKeyAt(at), getValueAt<ValueType>(at)};
    }

    void insertRange(NativeNaturalType at, NativeNaturalType elementCount) {
        NativeNaturalType newElementCount = getElementCount()+elementCount,
                          value = getChildBegin(at);
        Super::insertRange(at, elementCount);
        for(NativeNaturalType atEnd = at+elementCount; at < atEnd; ++at)
            Super::setValueAt(at, value);
        for(NativeNaturalType at = 0; at < newElementCount; ++at)
            Super::setValueAt(at, Super::getValueAt(at)+elementCount*sizeOfInBits<ElementType>::value);
    }

    template<bool childrenAsWell = true>
    void eraseRange(NativeNaturalType at, NativeNaturalType elementCount) {
        NativeNaturalType newElementCount = getElementCount()-elementCount,
                          sliceLength = getChildEnd(at+elementCount-1)-getChildBegin(at);
        if(childrenAsWell)
            Super::parent.decreaseChildLength(Super::childIndex, getChildOffset(at), sliceLength);
        Super::eraseRange(at, elementCount);
        for(NativeNaturalType at = 0; at < newElementCount; ++at)
            Super::setValueAt(at, Super::getValueAt(at)-elementCount*sizeOfInBits<ElementType>::value);
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
