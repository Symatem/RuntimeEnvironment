#include <External/Huffman.hpp>

struct ArithmeticCodecStaticModel {
    Natural32 symbolCount, totalFrequency;
    BlobVector<true, Natural32> symbolFrequencies, cumulativeFrequencies;

    ArithmeticCodecStaticModel(NativeNaturalType _symbolCount) :symbolCount(_symbolCount), totalFrequency(_symbolCount) {
        symbolFrequencies.reserve(symbolCount);
        cumulativeFrequencies.reserve(symbolCount);
        for(NativeNaturalType symbolIndex = 0; symbolIndex < symbolCount; ++symbolIndex) {
            symbolFrequencies.writeElementAt(symbolIndex, 1);
            cumulativeFrequencies.writeElementAt(symbolIndex, symbolIndex+1);
        }
    }

    Natural32 lowerFrequency(NativeNaturalType symbolIndex) {
        return (symbolIndex == 0) ? 0 : cumulativeFrequencies.readElementAt(symbolIndex-1);
    }

    Natural32 upperFrequency(NativeNaturalType symbolIndex) {
        return cumulativeFrequencies.readElementAt(symbolIndex);
    }

    void updateFrequency(NativeNaturalType symbolIndex) {
        totalFrequency = lowerFrequency(symbolIndex);
        for(; symbolIndex < symbolCount; ++symbolIndex) {
            totalFrequency += symbolFrequencies.readElementAt(symbolIndex);
            cumulativeFrequencies.writeElementAt(symbolIndex, totalFrequency);
        }
    }

    void update(NativeNaturalType symbolIndex) { }
};

struct ArithmeticCodecAdaptiveModel : public ArithmeticCodecStaticModel {
    ArithmeticCodecAdaptiveModel(NativeNaturalType _symbolCount) :ArithmeticCodecStaticModel(_symbolCount) { }

    void update(NativeNaturalType symbolIndex) {
        symbolFrequencies.writeElementAt(symbolIndex, symbolFrequencies.readElementAt(symbolIndex)+1);
        updateFrequency(symbolIndex);
    }
};

struct ArithmeticCodecSlidingWindowModel : public ArithmeticCodecStaticModel {
    BlobVector<true, Natural32> symbolRingBuffer;
    NativeNaturalType ringBufferIndex = 0;
    bool notFirstRound = false;

    ArithmeticCodecSlidingWindowModel(NativeNaturalType _symbolCount) :ArithmeticCodecStaticModel(_symbolCount) {
        symbolRingBuffer.reserve(4);
    }

    void update(NativeNaturalType incrementSymbolIndex) {
        NativeNaturalType decrementSymbolIndex = (notFirstRound) ? symbolRingBuffer.readElementAt(ringBufferIndex) : symbolCount;
        if(decrementSymbolIndex != incrementSymbolIndex) {
            if(notFirstRound)
                symbolFrequencies.writeElementAt(decrementSymbolIndex, symbolFrequencies.readElementAt(decrementSymbolIndex)-1);
            symbolFrequencies.writeElementAt(incrementSymbolIndex, symbolFrequencies.readElementAt(incrementSymbolIndex)+1);
            updateFrequency(min(decrementSymbolIndex, incrementSymbolIndex));
        }
        symbolRingBuffer.writeElementAt(ringBufferIndex, incrementSymbolIndex);
        ++ringBufferIndex;
        if(ringBufferIndex == symbolRingBuffer.size()) {
            ringBufferIndex = 0;
            notFirstRound = true;
        }
    }
};

struct ArithmeticCodec {
    ArithmeticCodecSlidingWindowModel model;

    Blob& blob;
    NativeNaturalType& offset;
    const Natural32 full = BitMask<Natural32>::full,
                    half = full/2+1, quarter = full/4+1;
    Natural64 low = 0, high = full, distance;

    ArithmeticCodec(Blob& _blob, NativeNaturalType& _offset, NativeNaturalType _symbolCount) :model(_symbolCount), blob(_blob), offset(_offset) { }

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

    ArithmeticEncoder(Blob& _blob, NativeNaturalType& _offset, NativeNaturalType _symbolCount) :ArithmeticCodec(_blob, _offset, _symbolCount) { }

    void encodeBit(bool bit) {
        blob.increaseSize(offset, 1);
        blob.externalOperate<true>(&bit, offset++, 1);
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

    ArithmeticDecoder(Blob& _blob, NativeNaturalType& _offset, NativeNaturalType _symbolCount) :ArithmeticCodec(_blob, _offset, _symbolCount), buffer(0) {
        const NativeNaturalType maxLength = BitMask<NativeNaturalType>::ceilLog2(full)-1;
        NativeNaturalType i;
        for(i = 0; decodeBit() && i < maxLength; ++i);
        buffer <<= maxLength-i;
    }

    bool decodeBit() {
        buffer <<= 1;
        if(offset == blob.getSize())
            return false;
        blob.externalOperate<false>(&buffer, offset++, 1);
        return true;
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
        NativeNaturalType symbolIndex = binarySearch<NativeNaturalType>(model.symbolCount, [&](NativeNaturalType at) {
            return frequency >= model.upperFrequency(at);
        });
        assert(symbolIndex < model.symbolCount);
        updateRange(symbolIndex);
        decoderNormalizeRange();
        model.update(symbolIndex);
        return symbolIndex;
    }
};

void arithmeticEncodeBlob(Blob& dst, Blob& src) {
    NativeNaturalType dstOffset = 0, srcLength = src.getSize(), bitsToSymbol = 4;
    dst.setSize(0);
    encodeBvlNatural(dst, dstOffset, srcLength);
    ArithmeticEncoder encoder(dst, dstOffset, 1<<bitsToSymbol);
    for(NativeNaturalType srcOffset = 0; srcOffset < srcLength; srcOffset += bitsToSymbol) {
        NativeNaturalType symbolIndex = 0;
        src.externalOperate<false>(&symbolIndex, srcOffset, bitsToSymbol);
        encoder.encodeSymbol(symbolIndex);
    }
    encoder.encodeTermination();
}

void arithmeticDecodeBlob(Blob& dst, Blob& src) {
    NativeNaturalType srcOffset = 0, dstLength = decodeBvlNatural(src, srcOffset), bitsToSymbol = 4;
    dst.setSize(dstLength);
    ArithmeticDecoder decoder(src, srcOffset, 1<<bitsToSymbol);
    for(NativeNaturalType dstOffset = 0; dstOffset < dstLength; dstOffset += bitsToSymbol) {
        NativeNaturalType symbolIndex = decoder.decodeSymbol();
        dst.externalOperate<true>(&symbolIndex, dstOffset, bitsToSymbol);
    }
}
