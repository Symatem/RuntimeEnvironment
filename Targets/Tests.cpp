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

void printBlob(Blob& blob) {
    NativeNaturalType length = blob.getSize(), offset = 0;
    while(offset < length) {
        NativeNaturalType buffer, sliceLength = min(static_cast<NativeNaturalType>(architectureSize), length-offset);
        blob.externalOperate<false>(&buffer, offset, sliceLength);
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

    test("BitstreamContainerGuard") {
        Symbol symbol;
        {
            BitstreamContainerGuard<BitstreamVector<VoidType>> container;
            symbol = container.blob.symbol;
            container.blob.setSize(32);
            assert(container.blob.getSize() == 32);
        }
        assert(Blob(symbol).getSize() == 0);
    }

    test("BitstreamVector") {
        BitstreamContainerGuard<BitstreamVector<NativeNaturalType>> vector;
        insertAsLastElement(vector, 2);
        insertAsFirstElement(vector, 1);
        vector.insertElementAt(1, 0);
        swapElementsAt(vector, 0, 1);
        assert(vector.getElementCount() == 3);
        NativeNaturalType index = 0;
        iterateElements(vector, [&](NativeNaturalType element) {
            assert(element == index++);
        });
        vector.moveElementAt(2, 0);
        vector.moveElementAt(0, 1);
        vector.eraseElementAt(1);
        assert(eraseLastElement(vector) == 0
            && eraseFirstElement(vector) == 2
            && vector.getElementCount() == 0);
        setElementCount(vector, 3);
        assert(vector.getElementCount() == 3);
        setElementCount(vector, 0);
        assert(vector.getElementCount() == 0);
    }

    test("BitstreamPairVector") {
        BitstreamContainerGuard<BitstreamPairVector<NativeNaturalType, NativeNaturalType>> pairVector;
        insertAsLastElement(pairVector, {2, 0});
        insertAsFirstElement(pairVector, {3, 1});
        pairVector.setKeyAt(0, 0);
        pairVector.setValueAt(1, 3);
        assert(pairVector.getElementCount() == 2);
        NativeNaturalType index = 0;
        iterateElements(pairVector, [&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index+1);
            index += 2;
        });
        index = 0;
        iterateKeys(pairVector, [&](NativeNaturalType key) {
            assert(key == index);
            index += 2;
        });
        index = 1;
        iterateValues(pairVector, [&](NativeNaturalType value) {
            assert(value == index);
            index += 2;
        });
    }

    test("BitstreamHeap") {
        BitstreamContainerGuard<BitstreamHeap<Ascending, NativeNaturalType, NativeNaturalType>> heap;
        heap.insertElement({2, 4});
        heap.insertElement({0, 0});
        heap.insertElement({4, 8});
        heap.insertElement({1, 2});
        heap.insertElement({3, 6});
        heap.build();
        assert(heap.getElementCount() == 5);
        NativeNaturalType index = 0;
        for(; index < 5; ++index) {
            Pair<NativeNaturalType, NativeNaturalType> element = eraseFirstElement(heap);
            assert(element.first == index && element.second == index*2);
        }
        heap.insertElement({2, 4});
        heap.insertElement({0, 0});
        heap.insertElement({4, 8});
        heap.insertElement({1, 2});
        heap.insertElement({3, 6});
        heap.reverseSort();
        assert(heap.getElementCount() == 5);
        iterateElements(heap, [&](Pair<NativeNaturalType, NativeNaturalType> element) {
            --index;
            assert(element.first == index && element.second == index*2);
        });
    }

    test("BitstreamSet") {
        BitstreamContainerGuard<BitstreamSet<NativeNaturalType, NativeNaturalType>> set;
        assert(set.insertElement({1, 2}) == true
            && set.insertElement({5, 0}) == true
            && set.insertElement({3, 6}) == true
            && set.insertElement({0, 8}) == true
            && set.insertElement({2, 4}) == true
            && set.getElementCount() == 5
            && set.setKeyAt(4, 1) == false
            && set.setKeyAt(0, 4) == true
            && set.setKeyAt(4, 0) == true);
        NativeNaturalType index = 0;
        iterateElements(set, [&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index*2);
            ++index;
        });
        assert(set.findKey(7, index) == false
            && set.findKey(2, index) == true && index == 2
            && set.eraseElement(7) == false
            && set.eraseElement(2) == true
            && set.eraseElement(4) == true
            && set.eraseElement(3) == true
            && set.getElementCount() == 2);
        index = 0;
        iterateElements(set, [&](Pair<NativeNaturalType, NativeNaturalType> element) {
            assert(element.first == index && element.second == index*2);
            ++index;
        });
    }

    test("BitstreamContainerVector") {
        BitstreamContainerGuard<BitstreamContainerVector<NativeNaturalType, VoidType>> containerVector;
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

    test("BitstreamContainerSet") {
        BitstreamContainerGuard<BitstreamContainerSet<NativeNaturalType, VoidType>> containerSet;
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
            && containerSet.eraseElement(8) == true
            && containerSet.getElementCount() == 2
            && containerSet.getChildLength(0) == 64
            && containerSet.getChildLength(1) == 32);
    }

    test("BitstreamPairSet") {
        BitstreamContainerGuard<BitstreamPairSet<NativeNaturalType, NativeNaturalType>> pairSet;
        assert(pairSet.insertElement({3, 1})
            && pairSet.insertElement({3, 3})
            && pairSet.insertElement({5, 5})
            && pairSet.insertElement({5, 7})
            && pairSet.insertElement({5, 9})
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

    test("ArithmeticCodec Symbol") {
        Blob blob(createSymbol());
        NativeNaturalType offset = 0;
        ArithmeticEncoder encoder(blob, offset, 3);
        for(NativeNaturalType i = 0; i < 8; ++i)
            encoder.encodeSymbol(i%3);
        encoder.encodeTermination();
        offset = 0;
        ArithmeticDecoder decoder(blob, offset, encoder.model.symbolCount);
        for(NativeNaturalType i = 0; i < 8; ++i)
            assert(decoder.decodeSymbol() == (i%3));
    }

    test("ArithmeticCodec Bitstream") {
        Blob plain(createFromString("Hello, World!")), compressed(createSymbol()), decompressed(createSymbol());
        decompressed.setSize(plain.getSize());
        NativeNaturalType offset = 0;
        arithmeticEncodeBlob(compressed, offset, plain, plain.getSize());
        offset = 0;
        arithmeticDecodeBlob(decompressed, decompressed.getSize(), compressed, offset);
        assert(compressed.compare(plain) != 0 && decompressed.compare(plain) == 0);
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
