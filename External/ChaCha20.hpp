#include <External/Arithmetic.hpp>

struct ChaCha20 {
    union {
        struct {
            Natural8 constant[16], key[32];
            Natural64 counter, nonce;
        };
        Natural32 block32[16];
        Natural64 block64[8];
    };

    static void quarterRound(Natural32& a, Natural32& b, Natural32& c, Natural32& d) {
        a += b; d ^= a; d = BitMask<Natural32>::barrelShiftMultiply(d, 16);
        c += d; b ^= c; b = BitMask<Natural32>::barrelShiftMultiply(b, 12);
        a += b; d ^= a; d = BitMask<Natural32>::barrelShiftMultiply(d, 8 );
        c += d; b ^= c; b = BitMask<Natural32>::barrelShiftMultiply(b, 7 );
    }

    void scramble(NativeNaturalType rounds = 10) {
        for(NativeNaturalType i = 0; i < rounds; ++i) {
            quarterRound(block32[0], block32[4], block32[ 8], block32[12]);
            quarterRound(block32[1], block32[5], block32[ 9], block32[13]);
            quarterRound(block32[2], block32[6], block32[10], block32[14]);
            quarterRound(block32[3], block32[7], block32[11], block32[15]);
            quarterRound(block32[0], block32[5], block32[10], block32[15]);
            quarterRound(block32[1], block32[6], block32[11], block32[12]);
            quarterRound(block32[2], block32[7], block32[ 8], block32[13]);
            quarterRound(block32[3], block32[4], block32[ 9], block32[14]);
        }
    }

    void generate(ChaCha20& context) {
        for(Natural8 j = 0; j < 8; ++j)
            block64[j] = context.block64[j];
        scramble();
        for(Natural8 j = 0; j < 8; ++j)
            block64[j] += context.block64[j];
        ++context.counter;
    }

    void applyOnBitVector(BitVector& bitVector) {
        ChaCha20 buffer, mask;
        NativeNaturalType endOffset = bitVector.getSize(), offset = 0;
        while(offset < endOffset) {
            NativeNaturalType sliceLength = min(endOffset-offset, sizeOfInBits<ChaCha20>::value);
            mask.generate(*this);
            bitVector.externalOperate<false>(&buffer, offset, sliceLength);
            for(Natural8 i = 0; i < 8; ++i)
                buffer.block64[i] ^= mask.block64[i];
            bitVector.externalOperate<true>(&buffer, offset, sliceLength);
            offset += sliceLength;
        }
    }
};

struct PseudoRandomGenerator {
    ChaCha20 context, buffer;
    Natural8 poolFillLevel;

    PseudoRandomGenerator() {
        memset(&context, 0, sizeof(ChaCha20));
        context.block64[0] = 1;
        poolFillLevel = 0;
    }

    Natural64 generateNatural() {
        if(poolFillLevel == 0) {
            buffer.generate(context);
            poolFillLevel = sizeof(ChaCha20)/sizeof(Natural64);
        }
        return buffer.block64[--poolFillLevel];
    }

    Integer64 generateInteger() {
        return static_cast<Integer64>(generateNatural());
    }

    Float64 generateFloat() {
        return static_cast<Float64>(generateNatural())/BitMask<Natural64>::full;
    }
};
