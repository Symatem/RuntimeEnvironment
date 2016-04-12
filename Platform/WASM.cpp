#include "../Interpreter/Procedures.hpp"

__attribute__((noinline)) unsigned getMemorySize() {
    unsigned result;
    asm("memory_size $0=");
    return result;
}

__attribute__((noinline)) void growMemory(unsigned delta) {
    asm("grow_memory $discard=, $0");
}

void Storage::resizeMemory(NativeNaturalType _pageCount) {
    assert(_pageCount < maxPageCount);
    pageCount = getMemorySize()/4096;
    if(_pageCount > pageCount)
        growMemory(_pageCount-pageCount);
    pageCount = _pageCount;
}

struct Main {
    Main() {
        Storage::ptr = reinterpret_cast<char*>(4096*10); // TODO
        Storage::resizeMemory(Storage::minPageCount);
        Ontology::tryToFillPreDefined();
    }
} startUp;
