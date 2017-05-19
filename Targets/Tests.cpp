#include <Targets/POSIX.hpp>

extern "C" {

#define test(name) beginTest(__COUNTER__, name);
static const char* currentTest = nullptr;
static NativeNaturalType testIndex = 0;
extern NativeNaturalType testCount;

void beginTest(NativeNaturalType index, const char* name) {
    if(currentTest)
        printf("\e[1m[ OK ]\e[0m %s\n", currentTest);
    testIndex = index;
    currentTest = name;
}

void assertFailed(const char* message) {
    printf("\e[1m[FAIL]\e[0m %s: %s\n", currentTest, message);
    printf("\e[0;31m\u2716 Test %" PrintFormatNatural "/%" PrintFormatNatural " failed\e[0m\n", testIndex, testCount);
    abort();
    // signal(SIGTRAP);
}

void printBitVector(BitVector& bitVector) {
    NativeNaturalType length = bitVector.getSize(), offset = 0;
    while(offset < length) {
        NativeNaturalType buffer, sliceLength = min(static_cast<NativeNaturalType>(architectureSize), length-offset);
        bitVector.externalOperate<false>(&buffer, offset, sliceLength);
        offset += sliceLength;
        for(NativeNaturalType i = 0; i < sliceLength; ++i)
            printf("%" PrintFormatNatural, (buffer>>i)&1);
    }
    printf("\n");
}

Integer32 main(Integer32 argc, Integer8** argv) {
    test("loadStorage()") {
        assert(argc == 2);
        loadStorage(argv[1]);
    }

    test("tryToFillPreDefined()") {
        tryToFillPreDefined();
    }

    test("BitVectorGuard<DataStructure>") {
        Symbol symbol;
        {
            BitVectorGuard<DataStructure<Vector<VoidType>>> container;
            symbol = container.getBitVector().symbol;
            container.getBitVector().setSize(32);
            assert(container.getBitVector().getSize() == 32);
        }
        assert(BitVector(symbol).getSize() == 0);
    }

    test("Vector") {
        BitVectorGuard<DataStructure<Vector<NativeNaturalType>>> vector;
        vector.insertAsLastElement(2);
        vector.insertAsFirstElement(1);
        vector.insertElementAt(1, 0);
        vector.swapElementsAt(0, 1);
        assert(vector.getElementCount() == 3);
        NativeNaturalType index = 0;
        vector.iterateElements([&](NativeNaturalType element) {
            assert(element == index++);
        });
        vector.moveElementAt(2, 0);
        vector.moveElementAt(0, 1);
        vector.eraseElementAt(1);
        assert(vector.eraseLastElement() == 0
            && vector.eraseFirstElement() == 2
            && vector.getElementCount() == 0);
        vector.setElementCount(3);
        assert(vector.getElementCount() == 3);
        vector.setElementCount(0);
        assert(vector.getElementCount() == 0);
    }

    test("PairVector") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<PairVector<ElementType::FirstType, ElementType::SecondType>>> pairVector;
        pairVector.insertAsLastElement(ElementType{2, 0});
        pairVector.insertAsFirstElement(ElementType{3, 1});
        pairVector.setKeyAt(0, 0);
        pairVector.setValueAt(1, 3);
        assert(pairVector.getElementCount() == 2);
        NativeNaturalType index = 0;
        pairVector.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index+1);
            index += 2;
        });
        index = 0;
        pairVector.iterate([&](NativeNaturalType at) {
            assert(pairVector.getKeyAt(at) == index);
            index += 2;
        });
        index = 1;
        pairVector.iterate([&](NativeNaturalType at) {
            assert(pairVector.getValueAt(at) == index);
            index += 2;
        });
    }

    test("Heap") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<Heap<Ascending, ElementType::FirstType, ElementType::SecondType>>> heap;
        heap.insertElement(ElementType{2, 4});
        heap.insertElement(ElementType{0, 0});
        heap.insertElement(ElementType{4, 8});
        heap.insertElement(ElementType{1, 2});
        heap.insertElement(ElementType{3, 6});
        heap.build();
        assert(heap.getElementCount() == 5);
        NativeNaturalType index = 0;
        for(; index < 5; ++index) {
            Pair<NativeNaturalType, NativeNaturalType> element = heap.eraseFirstElement();
            assert(element.first == index && element.second == index*2);
        }
        heap.insertElement(ElementType{2, 4});
        heap.insertElement(ElementType{0, 0});
        heap.insertElement(ElementType{4, 8});
        heap.insertElement(ElementType{1, 2});
        heap.insertElement(ElementType{3, 6});
        heap.reverseSort();
        assert(heap.getElementCount() == 5);
        heap.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            --index;
            assert(element.first == index && element.second == index*2);
        });
    }

    test("Set") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<Set<ElementType::FirstType, ElementType::SecondType>>> set;
        assert(set.insertElement(ElementType{1, 2}) == true
            && set.insertElement(ElementType{5, 0}) == true
            && set.insertElement(ElementType{3, 6}) == true
            && set.insertElement(ElementType{0, 8}) == true
            && set.insertElement(ElementType{2, 4}) == true
            && set.getElementCount() == 5
            && set.setKeyAt(4, 1) == false
            && set.setKeyAt(0, 4) == true
            && set.setKeyAt(4, 0) == true);
        NativeNaturalType index = 0;
        set.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index*2);
            ++index;
        });
        assert(set.findKey(7, index) == false
            && set.findKey(2, index) == true && index == 2
            && set.eraseElementByKey(7) == false
            && set.eraseElementByKey(2) == true
            && set.eraseElementByKey(4) == true
            && set.eraseElementByKey(3) == true
            && set.getElementCount() == 2);
        index = 0;
        set.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index*2);
            ++index;
        });
    }

    test("MetaVector") {
        BitVectorGuard<DataStructure<MetaVector<NativeNaturalType, VoidType>>> containerVector;
        containerVector.insertElementAt(0, 7);
        containerVector.increaseChildLength(0, containerVector.getChildOffset(0), 64);
        containerVector.insertElementAt(0, 5);
        containerVector.increaseChildLength(0, containerVector.getChildOffset(0), 32);
        containerVector.insertElementAt(1, 9);
        containerVector.increaseChildLength(1, containerVector.getChildOffset(1), 96);
        assert(containerVector.moveElementAt(2, 1) == true
            && containerVector.moveElementAt(0, 1) == true
            && containerVector.getElementCount() == 3
            && containerVector.getKeyAt(0) == 7 && containerVector.getChildLength(0) == 64
            && containerVector.getKeyAt(1) == 5 && containerVector.getChildLength(1) == 32
            && containerVector.getKeyAt(2) == 9 && containerVector.getChildLength(2) == 96);
        containerVector.eraseElementAt(1);
        assert(containerVector.getElementCount() == 2
            && containerVector.getKeyAt(0) == 7 && containerVector.getChildLength(0) == 64
            && containerVector.getKeyAt(1) == 9 && containerVector.getChildLength(1) == 96);
    }

    test("MetaSet") {
        BitVectorGuard<DataStructure<MetaSet<NativeNaturalType, VoidType>>> containerSet;
        assert(containerSet.insertElement(7) == true);
        containerSet.increaseChildLength(0, containerSet.getChildOffset(0), 64);
        assert(containerSet.insertElement(5) == true);
        containerSet.increaseChildLength(0, containerSet.getChildOffset(0), 32);
        assert(containerSet.insertElement(8) == true);
        containerSet.increaseChildLength(2, containerSet.getChildOffset(2), 96);
        assert(containerSet.getElementCount() == 3
            && containerSet.getChildLength(0) == 32
            && containerSet.getChildLength(1) == 64
            && containerSet.getChildLength(2) == 96
            && containerSet.setKeyAt(1, 4)
            && containerSet.setKeyAt(1, 9)
            && containerSet.eraseElementByKey(8) == true
            && containerSet.getElementCount() == 2
            && containerSet.getChildLength(0) == 64
            && containerSet.getChildLength(1) == 32);
    }

    test("PairSet") {
        typedef Pair<NativeNaturalType, NativeNaturalType> ElementType;
        BitVectorGuard<DataStructure<PairSet<ElementType::FirstType, ElementType::SecondType>>> pairSet;
        assert(pairSet.insertElement(ElementType{3, 1})
            && pairSet.insertElement(ElementType{3, 3})
            && pairSet.insertElement(ElementType{5, 5})
            && pairSet.insertElement(ElementType{5, 7})
            && pairSet.insertElement(ElementType{5, 9})
            && pairSet.getFirstKeyCount() == 2
            && pairSet.getSecondKeyCount(0) == 2
            && pairSet.getSecondKeyCount(1) == 3);
        NativeNaturalType firstAt, secondAt;
        assert(pairSet.findFirstKey(1, firstAt) == false
            && pairSet.findFirstKey(5, firstAt) == true && firstAt == 1
            && pairSet.findSecondKey(1, firstAt, secondAt) == false
            && pairSet.findSecondKey(5, firstAt, secondAt) == true && secondAt == 0
            && pairSet.findElement({1, 1}, firstAt, secondAt) == false
            && pairSet.findElement({3, 5}, firstAt, secondAt) == false
            && pairSet.findElement({3, 3}, firstAt, secondAt) == true && firstAt == 0 && secondAt == 1);
        assert(pairSet.eraseElement({5, 9}) == true);
        NativeNaturalType counter = 1;
        pairSet.iterateFirstKeys([&](NativeNaturalType first) {
            counter += 2;
            assert(first == counter);
        });
        pairSet.iterateSecondKeys(1, [&](NativeNaturalType second) {
            assert(second == counter);
            counter += 2;
        });
        counter = 1;
        pairSet.iterateElements([&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == (counter-1)/4*2+3 && element.second == counter);
            counter += 2;
        });
    }

    test("BitMap fillSlice and clearSlice") {
        BitVectorGuard<DataStructure<BitMap<>>> bitMap;
        NativeNaturalType sliceIndex;
        assert(bitMap.getElementCount() == 0);
        bitMap.fillSlice(64, 16);
        assert(bitMap.getSliceContaining(0, sliceIndex) == false && sliceIndex == 0
            && bitMap.getSliceContaining(64, sliceIndex) == false && sliceIndex == 0
            && bitMap.getSliceContaining(65, sliceIndex) == true && sliceIndex == 0
            && bitMap.getSliceContaining(80, sliceIndex) == false && sliceIndex == 1);
        bitMap.fillSlice(0, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 16
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 16);
        bitMap.fillSlice(8, 16);
        bitMap.fillSlice(56, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 24
            && bitMap.getSliceBeginAddress(1) == 56 && bitMap.getChildLength(1) == 24);
        bitMap.fillSlice(24, 8);
        bitMap.fillSlice(48, 8);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 32
            && bitMap.getSliceBeginAddress(1) == 48 && bitMap.getChildLength(1) == 32);
        bitMap.fillSlice(24, 32);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 80);
        bitMap.clearSlice(32, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 32
            && bitMap.getSliceBeginAddress(1) == 48 && bitMap.getChildLength(1) == 32);
        bitMap.clearSlice(24, 8);
        bitMap.clearSlice(48, 8);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 24
            && bitMap.getSliceBeginAddress(1) == 56 && bitMap.getChildLength(1) == 24);
        bitMap.clearSlice(16, 16);
        bitMap.clearSlice(48, 16);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 0 && bitMap.getChildLength(0) == 16
            && bitMap.getSliceBeginAddress(1) == 64 && bitMap.getChildLength(1) == 16);
        bitMap.clearSlice(64, 16);
        bitMap.clearSlice(0, 16);
        assert(bitMap.getElementCount() == 0);
    }

    test("BitMap moveSlice") {
        BitVectorGuard<DataStructure<BitMap<>>> bitMap;
        bitMap.fillSlice(16, 16);
        assert(bitMap.moveSlice(32, 16, 8) == false
            && bitMap.moveSlice(32, 24, 8) == true);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 32 && bitMap.getChildLength(1) == 8);
        bitMap.moveSlice(24, 16, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 24 && bitMap.getChildLength(0) == 16);
        bitMap.moveSlice(32, 24, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 32 && bitMap.getChildLength(0) == 8);
        bitMap.fillSlice(24, 8);
        assert(bitMap.moveSlice(16, 32, 8) == false
            && bitMap.moveSlice(16, 24, 8) == true);
        assert(bitMap.getElementCount() == 2
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8
            && bitMap.getSliceBeginAddress(1) == 32 && bitMap.getChildLength(1) == 8);
        bitMap.moveSlice(24, 32, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 16);
        bitMap.moveSlice(16, 24, 8);
        assert(bitMap.getElementCount() == 1
            && bitMap.getSliceBeginAddress(0) == 16 && bitMap.getChildLength(0) == 8);
    }

    test("ArithmeticCodec Symbol") {
        BitVector bitVector(createSymbol());
        NativeNaturalType offset = 0;
        ArithmeticEncoder encoder(bitVector, offset, 3);
        for(NativeNaturalType i = 0; i < 8; ++i)
            encoder.encodeSymbol(i%3);
        encoder.encodeTermination();
        offset = 0;
        ArithmeticDecoder decoder(bitVector, offset, encoder.model.symbolCount);
        for(NativeNaturalType i = 0; i < 8; ++i)
            assert(decoder.decodeSymbol() == (i%3));
    }

    test("ArithmeticCodec BitVector") {
        BitVector plain(createFromString("Hello, World!")), compressed(createSymbol()), decompressed(createSymbol());
        decompressed.setSize(plain.getSize());
        NativeNaturalType offset = 0;
        arithmeticEncodeBitVector(compressed, offset, plain, 0, plain.getSize());
        offset = 0;
        arithmeticDecodeBitVector(decompressed, 0, decompressed.getSize(), compressed, offset);
        assert(compressed.compare(plain) != 0 && decompressed.compare(plain) == 0);
    }

    test("Triple") {
        Triple triple = {1, 2, 3};
        assert(query(QueryMask::MMM, triple) == 0);
        assert(link(triple) == true);
        assert(query(QueryMask::MMM, triple) == 1);
        assert(unlink(triple) == true);
        assert(query(QueryMask::MMM, triple) == 0);
    }

    test("unloadStorage()") {
        unloadStorage();
    }

    beginTest(testCount, nullptr);
    printf("\e[0;32m\u2714 All %" PrintFormatNatural " tests succeeded\e[0m\n", testCount);
    return 0;
}

NativeNaturalType testCount = __COUNTER__;

}
