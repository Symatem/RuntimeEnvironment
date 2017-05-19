#include <External/Huffman.hpp>

struct ArithmeticCodecStaticModel {
    Natural32 symbolCount, totalFrequency;
    BitVectorGuard<DataStructure<Vector<Natural32>>> symbolFrequencies, cumulativeFrequencies;

    ArithmeticCodecStaticModel(NativeNaturalType _symbolCount)
        :symbolCount(_symbolCount), totalFrequency(_symbolCount) {
        symbolFrequencies.setElementCount(symbolCount);
        cumulativeFrequencies.setElementCount(symbolCount);
        for(NativeNaturalType symbolIndex = 0; symbolIndex < symbolCount; ++symbolIndex) {
            symbolFrequencies.setElementAt(symbolIndex, 1);
            cumulativeFrequencies.setElementAt(symbolIndex, symbolIndex+1);
        }
    }

    Natural32 lowerFrequency(NativeNaturalType symbolIndex) {
        return (symbolIndex == 0) ? 0 : cumulativeFrequencies.getElementAt(symbolIndex-1);
    }

    Natural32 upperFrequency(NativeNaturalType symbolIndex) {
        return cumulativeFrequencies.getElementAt(symbolIndex);
    }

    void updateFrequency(NativeNaturalType symbolIndex) {
        totalFrequency = lowerFrequency(symbolIndex);
        for(; symbolIndex < symbolCount; ++symbolIndex) {
            totalFrequency += symbolFrequencies.getElementAt(symbolIndex);
            cumulativeFrequencies.setElementAt(symbolIndex, totalFrequency);
        }
    }

    void update(NativeNaturalType symbolIndex) {}
};

struct ArithmeticCodecAdaptiveModel : public ArithmeticCodecStaticModel {
    ArithmeticCodecAdaptiveModel(NativeNaturalType _symbolCount) :ArithmeticCodecStaticModel(_symbolCount) {}

    void update(NativeNaturalType symbolIndex) {
        symbolFrequencies.setElementAt(symbolIndex, symbolFrequencies.getElementAt(symbolIndex)+1);
        updateFrequency(symbolIndex);
    }
};

struct ArithmeticCodecSlidingWindowModel : public ArithmeticCodecStaticModel {
    BitVectorGuard<DataStructure<Vector<Natural32>>> symbolRingBuffer;
    NativeNaturalType ringBufferIndex = 0;
    bool notFirstRound = false;

    ArithmeticCodecSlidingWindowModel(NativeNaturalType _symbolCount)
        :ArithmeticCodecStaticModel(_symbolCount) {
        symbolRingBuffer.setElementCount(4);
    }

    void update(NativeNaturalType incrementSymbolIndex) {
        NativeNaturalType decrementSymbolIndex = (notFirstRound) ? symbolRingBuffer.getElementAt(ringBufferIndex) : symbolCount;
        if(decrementSymbolIndex != incrementSymbolIndex) {
            if(notFirstRound)
                symbolFrequencies.setElementAt(decrementSymbolIndex, symbolFrequencies.getElementAt(decrementSymbolIndex)-1);
            symbolFrequencies.setElementAt(incrementSymbolIndex, symbolFrequencies.getElementAt(incrementSymbolIndex)+1);
            updateFrequency(min(decrementSymbolIndex, incrementSymbolIndex));
        }
        symbolRingBuffer.setElementAt(ringBufferIndex, incrementSymbolIndex);
        ++ringBufferIndex;
        if(ringBufferIndex == symbolRingBuffer.getElementCount()) {
            ringBufferIndex = 0;
            notFirstRound = true;
        }
    }
};

struct ArithmeticCodec {
    ArithmeticCodecSlidingWindowModel model;

    BitVector& bitVector;
    NativeNaturalType& offset;
    const Natural32 full = BitMask<Natural32>::full,
                    half = full/2+1, quarter = full/4+1;
    Natural64 low = 0, high = full, distance;

    ArithmeticCodec(BitVector& _bitVector, NativeNaturalType& _offset, NativeNaturalType _symbolCount)
        :model(_symbolCount), bitVector(_bitVector), offset(_offset) {}

    void updateRange(NativeNaturalType symbolIndex) {
        Natural32 lowerFrequency = model.lowerFrequency(symbolIndex),
                  upperFrequency = model.upperFrequency(symbolIndex);
        high = low+(distance*upperFrequency/model.totalFrequency)-1;
        low  = low+(distance*lowerFrequency/model.totalFrequency);
        assert(low < high);
    }

    void shiftAndScaleRange(Natural32 shift) {
        low -= shift;
        high -= shift;
        low <<= 1;
        high <<= 1;
        high |= 1;
    }

    Natural8 normalizeRange() {
        if(high < half) {
            shiftAndScaleRange(0);
            return 0;
        } else if(low >= half) {
            shiftAndScaleRange(half);
            return 1;
        } else if(low >= quarter && high < half+quarter) {
            shiftAndScaleRange(quarter);
            return 2;
        } else
            return 3;
    }
};

struct ArithmeticEncoder : public ArithmeticCodec {
    NativeNaturalType underflowCounter = 0;

    ArithmeticEncoder(BitVector& _bitVector, NativeNaturalType& _offset, NativeNaturalType _symbolCount)
        :ArithmeticCodec(_bitVector, _offset, _symbolCount) {}

    void encodeBit(bool bit) {
        bitVector.increaseSize(offset, 1);
        bitVector.externalOperate<true>(&bit, offset++, 1);
    }

    void encoderNormalizeRange() {
        while(true) {
            Natural8 operation = normalizeRange();
            switch(operation) {
                case 0:
                case 1:
                    encodeBit(operation);
                    for(; underflowCounter > 0; --underflowCounter)
                        encodeBit(!operation);
                    break;
                case 2:
                    ++underflowCounter;
                    break;
                case 3:
                    return;
            }
        }
    }

    void encodeSymbol(NativeNaturalType symbolIndex) {
        distance = high-low+1;
        updateRange(symbolIndex);
        encoderNormalizeRange();
        model.update(symbolIndex);
    }

    void encodeTermination() {
        if(low < quarter) {
            encodeBit(0);
            encodeBit(1);
            for(; underflowCounter > 0; --underflowCounter)
                encodeBit(1);
        } else
            encodeBit(1);
    }
};

struct ArithmeticDecoder : public ArithmeticCodec {
    Natural32 buffer;
    NativeNaturalType offsetCorrection;

    ArithmeticDecoder(BitVector& _bitVector, NativeNaturalType& _offset, NativeNaturalType _symbolCount)
        :ArithmeticCodec(_bitVector, _offset, _symbolCount), buffer(0) {
        const NativeNaturalType maxLength = BitMask<NativeNaturalType>::ceilLog2(full)-1;
        for(offsetCorrection = 0; decodeBit() && offsetCorrection < maxLength; ++offsetCorrection);
        buffer <<= maxLength-offsetCorrection;
    }

    bool decodeBit() {
        buffer <<= 1;
        if(offset >= bitVector.getSize()) {
            ++offset;
            return false;
        } else {
            bitVector.externalOperate<false>(&buffer, offset++, 1);
            return true;
        }
    }

    void decoderNormalizeRange() {
        while(true) {
            Natural8 operation = normalizeRange();
            if(operation == 3)
                return;
            decodeBit();
            if(operation == 2)
                buffer ^= half;
        }
    }

    NativeNaturalType decodeSymbol() {
        assert(low <= buffer && buffer <= high);
        distance = high-low+1;
        Natural64 frequency = buffer-low+1;
        frequency *= model.totalFrequency;
        frequency /= distance;
        NativeNaturalType symbolIndex = binarySearch<NativeNaturalType>(0, model.symbolCount, [&](NativeNaturalType at) {
            return frequency >= model.upperFrequency(at);
        });
        assert(symbolIndex < model.symbolCount);
        updateRange(symbolIndex);
        decoderNormalizeRange();
        model.update(symbolIndex);
        return symbolIndex;
    }

    void decodeTermination() {
        offset -= offsetCorrection;
        if(low < quarter)
            ++offset;
    }
};

void arithmeticEncodeBitVector(BitVector& dst, NativeNaturalType& dstOffset,
        BitVector& src, NativeNaturalType srcOffset, NativeNaturalType srcLength,
        NativeNaturalType bitsToSymbol = 4) {
    assert(srcLength%bitsToSymbol == 0);
    ArithmeticEncoder encoder(dst, dstOffset, 1<<bitsToSymbol);
    for(NativeNaturalType offset = srcOffset; offset < srcOffset+srcLength; offset += bitsToSymbol) {
        NativeNaturalType symbolIndex = 0;
        src.externalOperate<false>(&symbolIndex, offset, bitsToSymbol);
        encoder.encodeSymbol(symbolIndex);
    }
    encoder.encodeTermination();
}

void arithmeticDecodeBitVector(BitVector& dst, NativeNaturalType dstOffset, NativeNaturalType dstLength,
        BitVector& src, NativeNaturalType& srcOffset,
        NativeNaturalType bitsToSymbol = 4) {
    assert(dstLength%bitsToSymbol == 0);
    ArithmeticDecoder decoder(src, srcOffset, 1<<bitsToSymbol);
    for(NativeNaturalType offset = dstOffset; offset < dstOffset+dstLength; offset += bitsToSymbol) {
        NativeNaturalType symbolIndex = decoder.decodeSymbol();
        dst.externalOperate<true>(&symbolIndex, offset, bitsToSymbol);
    }
    decoder.decodeTermination();
}
