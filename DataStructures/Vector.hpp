#include <DataStructures/DataStructures.hpp>

template<typename _ElementType, typename _ParentType = BitVectorContainer>
struct Vector {
    typedef _ElementType ElementType;
    typedef _ParentType ParentType;
    ParentType& parent;
    NativeNaturalType childIndex;

    Vector(ParentType& _parent, NativeNaturalType _childIndex = 0) :parent(_parent), childIndex(_childIndex) { }
    usingRemappedMethod(setElementCount)
    usingRemappedMethod(swapElementsAt)
    usingRemappedMethod(iterateElements)
    usingRemappedMethod(iterate)
    usingRemappedMethod(getFirstElement)
    usingRemappedMethod(getLastElement)
    usingRemappedMethod(insertAsFirstElement)
    usingRemappedMethod(insertAsLastElement)
    usingRemappedMethod(getAndEraseElementAt)
    usingRemappedMethod(eraseFirstElement)
    usingRemappedMethod(eraseLastElement)

    BitVector& getBitVector() {
        return parent.getBitVector();
    }

    bool operator==(Vector<ElementType, ParentType>& other) {
        return childIndex == other.childIndex && getBitVector().location == other.getBitVector().location;
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
        parent.increaseSize(getOffsetOfElement(at), elementCount*sizeOfInBits<ElementType>::value, childIndex);
    }

    void eraseRange(NativeNaturalType at, NativeNaturalType elementCount) {
        parent.decreaseSize(getOffsetOfElement(at), elementCount*sizeOfInBits<ElementType>::value, childIndex);
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
