#include <DataStructures/MetaVector.hpp>

template<typename KeyType, typename _ParentType = BitVectorContainer>
struct MetaSet : public MetaVector<KeyType, _ParentType> {
    typedef _ParentType ParentType;
    typedef MetaVector<KeyType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    MetaSet(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }
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
