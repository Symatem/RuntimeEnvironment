#include <Storage/Containers.hpp>

bool unlink(Symbol symbol);

template<bool guarded>
struct BlobIndex : public BlobSet<guarded, Symbol> {
    typedef BlobSet<guarded, Symbol> Super;

    BlobIndex() :Super() {}
    BlobIndex(const BlobIndex<false>& other) :Super(other) {}
    BlobIndex(const BlobIndex<true>& other) :Super(other) {}

    NativeNaturalType find(Symbol key) const {
        return binarySearch<NativeNaturalType>(Super::size(), [&](NativeNaturalType at) {
            return Blob(key).compare(Blob(Super::readElementAt(at))) < 0;
        });
    }

    bool find(Symbol element, NativeNaturalType& at) const {
        at = find(element);
        if(at == Super::size())
            return false;
        return (Blob(element).compare(Blob(Super::readElementAt(at))) == 0);
    }

    void insertElement(Symbol& element) {
        NativeNaturalType at;
        if(find(element, at)) {
            unlink(element);
            element = Super::readElementAt(at);
        } else
            Super::insert(at, element);
    }

    bool eraseElement(Symbol element) {
        NativeNaturalType at;
        if(!find(element, at))
            return false;
        Super::erase(at);
        return true;
    }
};

BlobIndex<false> blobIndex;
