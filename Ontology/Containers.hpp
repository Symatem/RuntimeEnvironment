#include "Context.hpp"

template<typename T>
struct Vector {
    Symbol symbol;
    SymbolObject* symbolObject;
    Context& context;

    Vector(Context& _context) :context(_context), symbol(_context.create()), symbolObject(_context.getSymbolObject(symbol)) {

    }

    ~Vector() {
        context.destroy(symbol);
    }

    bool empty() const {
        return symbolObject->blobSize == 0;
    }

    ArchitectureType size() const {
        return symbolObject->blobSize/(sizeof(T)*8);
    }

    T& operator[](ArchitectureType at) const {
        return symbolObject->template accessBlobAt<T>(at);
    }

    T& front() const {
        return (*this)[0];
    }

    T& back() const {
        return (*this)[size()-1];
    }

    void clear() {
        symbolObject->allocateBlob(0);
    }

    void push_back(T element) {
        symbolObject->reallocateBlob(symbolObject->blobSize+sizeof(T)*8);
        back() = element;
    }

    T pop_back() {
        assert(symbolObject->blobSize >= sizeof(T)*8);
        T element = back();
        symbolObject->reallocateBlob(symbolObject->blobSize-sizeof(T)*8);
        return element;
    }

    void insert(ArchitectureType at, T element) {
        symbolObject->insertIntoBlob(reinterpret_cast<ArchitectureType*>(&element), at*sizeof(T)*8, sizeof(T)*8);
    }

    void erase(ArchitectureType begin, ArchitectureType end) {
        symbolObject->eraseFromBlob(begin*sizeof(T)*8, end*sizeof(T)*8);
    }

    void erase(ArchitectureType at) {
        symbolObject->eraseFromBlob(at, at+1);
    }
};

template<typename T>
struct Set : public Vector<T> {
    ArchitectureType blobFindIndexFor(T key) const {
        ArchitectureType begin = 0, mid, end = Vector<T>::size();
        while(begin < end) {
            mid = (begin+end)/2;
            if(key > (*this)[mid])
                begin = mid+1;
            else
                end = mid;
        }
        return begin;
    }

    void insert(T element) {
        ArchitectureType at = blobFindIndexFor(element);
        Vector<T>::insert(at, element);
    }
};
