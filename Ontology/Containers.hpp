#include "Context.hpp"

template<typename T, bool guarded = true>
struct Vector {
    Symbol symbol;
    SymbolObject* symbolObject;
    Context& context;

    Vector(Context& _context)
        :context(_context), symbol(PreDef_Void), symbolObject(NULL) { }

    ~Vector() {
        if(guarded && symbolObject)
            context.destroy(symbol);
    }

    bool empty() const {
        return (!symbolObject || symbolObject->blobSize == 0);
    }

    ArchitectureType size() const {
        return (symbolObject) ? symbolObject->blobSize/(sizeof(T)*8) : 0;
    }

    T& operator[](ArchitectureType at) const {
        assert(symbolObject);
        return symbolObject->template accessBlobAt<T>(at);
    }

    T& front() const {
        return (*this)[0];
    }

    T& back() const {
        return (*this)[size()-1];
    }

    void iterate(std::function<void(T)> callback) const {
        for(ArchitectureType at = 0; at < size(); ++at)
            callback((*this)[at]);
    }

    void setSymbol(Symbol _symbol) {
        symbol = _symbol;
        symbolObject = context.getSymbolObject(symbol);
    }

    void activate() {
        if(!symbolObject)
            setSymbol(context.create());
    }

    void clear() {
        if(symbolObject)
            symbolObject->allocateBlob(0);
    }

    void push_back(T element) {
        activate();
        symbolObject->reallocateBlob(symbolObject->blobSize+sizeof(T)*8);
        back() = element;
    }

    T pop_back() {
        activate();
        assert(symbolObject->blobSize >= sizeof(T)*8);
        T element = back();
        symbolObject->reallocateBlob(symbolObject->blobSize-sizeof(T)*8);
        return element;
    }

    void insert(ArchitectureType at, T element) {
        activate();
        symbolObject->insertIntoBlob(reinterpret_cast<ArchitectureType*>(&element), at*sizeof(T)*8, sizeof(T)*8);
    }

    void erase(ArchitectureType begin, ArchitectureType end) {
        assert(symbolObject);
        symbolObject->eraseFromBlob(begin*sizeof(T)*8, end*sizeof(T)*8);
    }

    void erase(ArchitectureType at) {
        assert(symbolObject);
        symbolObject->eraseFromBlob(at, at+1);
    }
};

template<typename T, bool guarded = true>
struct Set : public Vector<T, guarded> {
    Set(Context& _context) :Vector<T, guarded>(_context) {

    }

    ArchitectureType blobFindIndexFor(T key) const {
        ArchitectureType begin = 0, mid, end = Vector<T, guarded>::size();
        while(begin < end) {
            mid = (begin+end)/2;
            if(key > (*this)[mid])
                begin = mid+1;
            else
                end = mid;
        }
        return begin;
    }

    bool insert(T element) {
        ArchitectureType at = blobFindIndexFor(element);
        if(at < Vector<T, guarded>::size() && (*this)[at] == element)
            return false;
        Vector<T, guarded>::insert(at, element);
        return true;
    }
};

ArchitectureType Context::searchGIV(ArchitectureType index, Triple& triple, std::function<void()> callback) {
    auto topIter = topIndex.find(triple.pos[0]);
    if(topIter == topIndex.end())
        throw Exception("Symbol is Nonexistent");
    auto& subIndex = topIter->second->subIndices[index];
    Set<Symbol> result(*this);
    for(auto& beta : subIndex)
        for(auto& gamma : beta.second)
            result.insert(gamma);
    if(callback)
        result.iterate([&](Symbol gamma) {
            triple.pos[2] = gamma;
            callback();
        });
    return result.size();
}

void Context::destroy(Symbol alpha) {
    auto topIter = topIndex.find(alpha);
    if(topIter == topIndex.end())
        throw Exception("Already destroyed", {
            {PreDef_Entity, alpha}
        });
    Set<Symbol> symbols(*this);
    for(ArchitectureType i = EAV; i <= VEA; ++i)
        for(auto& beta : topIter->second->subIndices[i])
            for(auto gamma : beta.second) {
                unlinkInternal(Triple(alpha, beta.first, gamma).normalized(i), true, alpha);
                symbols.insert(beta.first);
                symbols.insert(gamma);
            }
    topIndex.erase(topIter);
    symbols.iterate([&](Symbol symbol) {
        scrutinizeSymbol(symbol);
    });
}

void Context::scrutinizeHeldBy(Symbol symbol) {
    Set<Symbol> symbols(*this);
    symbols.insert(symbol);
    while(!symbols.empty()) {
        symbol = symbols.pop_back();
        if(topIndex.find(symbol) == topIndex.end() ||
           query(1, {PreDef_Void, PreDef_Holds, symbol}) > 0)
            continue;
        query(9, {symbol, PreDef_Holds, PreDef_Void}, [&](Triple result, ArchitectureType) {
            symbols.insert(result.pos[0]);
        });
        destroy(symbol);
    }
}

void Context::setSolitary(Triple triple, bool linkVoid) {
    bool toLink = (linkVoid || triple.value != PreDef_Void);
    Set<Symbol> symbols(*this);
    query(9, triple, [&](Triple result, ArchitectureType) {
        if((triple.pos[2] == result.pos[0]) && (linkVoid || result.pos[0] != PreDef_Void))
            toLink = false;
        else
            symbols.insert(result.pos[0]);
    });
    if(toLink)
        link(triple);
    symbols.iterate([&](Symbol symbol) {
        unlinkInternal({triple.pos[0], triple.pos[1], symbol});
    });
    if(!linkVoid)
        symbols.insert(triple.pos[0]);
    symbols.insert(triple.pos[1]);
    symbols.iterate([&](Symbol symbol) {
        scrutinizeSymbol(symbol);
    });
}
