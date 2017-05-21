#include <DataStructures/Heap.hpp>

template<typename _Super, typename KeyType, typename ValueType, typename _ParentType>
struct SetTemplate : public _Super {
    typedef _Super Super;
    typedef _ParentType ParentType;
    typedef typename Super::ElementType ElementType;

    SetTemplate(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) {}
    usingRemappedMethod(insertElement)
    usingRemappedMethod(eraseElementByKey)

    bool findKey(KeyType key, NativeNaturalType& at) {
        NativeNaturalType elementCount = Super::getElementCount();
        at = binarySearch<NativeNaturalType>(0, elementCount, [&](NativeNaturalType at) {
            return Super::getKeyAt(at) < key;
        });
        return (at < elementCount && Super::getKeyAt(at) == key);
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

template<typename KeyType, typename ValueType = VoidType, typename _ParentType = BitVectorContainer>
struct Set : public SetTemplate<PairVector<KeyType, ValueType, _ParentType>, KeyType, ValueType, _ParentType> {
    typedef SetTemplate<PairVector<KeyType, ValueType, _ParentType>, KeyType, ValueType, _ParentType> Super;
    using Super::Super;
};
