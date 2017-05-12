#include <Storage/DataStructures.hpp>

bool unlink(Symbol symbol);

template<typename _ParentType = BitstreamContainer>
struct BitstreamContentIndex : public BitstreamSet<Symbol, VoidType, _ParentType> {
    typedef _ParentType ParentType;
    typedef BitstreamSet<Symbol, VoidType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    BitstreamContentIndex(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

    bool findKey(Symbol key, NativeNaturalType& at) {
        at = binarySearch<NativeNaturalType>(0, Super::getElementCount(), [&](NativeNaturalType at) {
            return Blob(key).compare(Blob(Super::getElementAt(at))) < 0;
        });
        return (at < Super::getElementCount() && Super::getKeyAt(at) == key);
    }

    void insertElement(Symbol& element) {
        NativeNaturalType at;
        if(findKey(element, at)) {
            unlink(element);
            element = Super::getElementAt(at);
        } else
            Super::insertElementAt(at, element);
    }
};

BitstreamDataStructure<BitstreamContentIndex<>> blobIndex;
