#include <Foundation/Bitwise.hpp>

template<bool atEnd = false>
bool substrEqual(const char* a, const char* b) {
    NativeNaturalType aOffset, aLen = strlen(a), bLen = strlen(b);
    if(atEnd) {
        if(aLen < bLen)
            return false;
        aOffset = (aLen-bLen)*8;
    } else if(aLen != bLen)
        return false;
    else
        aOffset = 0;
    return bitwiseCompare(reinterpret_cast<const NativeNaturalType*>(a),
                          reinterpret_cast<const NativeNaturalType*>(b),
                          aOffset, 0, bLen*8) == 0;
}

template<typename IndexType>
IndexType binarySearch(IndexType end, Closure<bool(IndexType)> compare) {
    IndexType begin = 0, mid;
    while(begin < end) {
        mid = (begin+end)/2;
        if(compare(mid))
            begin = mid+1;
        else
            end = mid;
    }
    return begin;
}

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
