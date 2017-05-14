#include <Storage/DataStructures.hpp>

bool unlink(Symbol symbol);

template<typename _ParentType = BitVectorContainer>
struct ContentIndex : public Set<Symbol, VoidType, _ParentType> {
    typedef _ParentType ParentType;
    typedef Set<Symbol, VoidType, ParentType> Super;
    typedef typename Super::ElementType ElementType;

    ContentIndex(ParentType& _parent, NativeNaturalType _childIndex = 0) :Super(_parent, _childIndex) { }

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

DataStructure<ContentIndex<>> blobIndex;



struct OntologyStruct {
    Symbol subIndices[6], bitMap;

    auto getSubIndex(NativeNaturalType subIndex) {
        return DataStructure<PairSet<Symbol, Symbol>>(subIndices[subIndex]);
    }

    auto getBitMap() {
        return DataStructure<BitMap<>>(bitMap);
    }
};

DataStructure<Set<Symbol, OntologyStruct>> tripleIndex;
