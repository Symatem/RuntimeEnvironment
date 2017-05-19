#include <DataStructures/PairVector.hpp>

template<typename SortDirection, typename KeyType, typename ValueType = VoidType, typename _ParentType = BitVectorContainer>
struct Heap : public PairVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef PairVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    Heap(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }
    usingRemappedMethod(getAndEraseElementAt)
    usingRemappedMethod(eraseFirstElement)
    usingRemappedMethod(eraseLastElement)

    bool compare(NativeNaturalType atA, NativeNaturalType atB) {
        return SortDirection::compare(Super::getKeyAt(atA), Super::getKeyAt(atB));
    }

    void siftToRoot(NativeNaturalType at) {
        while(at > 0) {
            NativeNaturalType parent = (at-1)/2;
            if(!compare(at, parent))
                break;
            Super::swapElementsAt(at, parent);
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
            Super::swapElementsAt(at, min);
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
            Super::swapElementsAt(0, elementCount);
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
