#include <DataStructures/Heap.hpp>

template<typename KeyType, typename ValueType = VoidType, typename _ParentType = BitVectorContainer>
struct Set : public PairVector<KeyType, ValueType, _ParentType> {
    typedef _ParentType ParentType;
    typedef PairVector<KeyType, ValueType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    Set(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) {}
    usingRemappedMethod(insertElement)
    usingRemappedMethod(eraseElementByKey)

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
