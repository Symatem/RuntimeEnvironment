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
        Natural32 block[16];
    };

    static void quarterRound(Natural32& a, Natural32& b, Natural32& c, Natural32& d) {
        a += b; d ^= a; d = BitMask<Natural32>::barrelShiftMultiply(d, 16);
        c += d; b ^= c; b = BitMask<Natural32>::barrelShiftMultiply(b, 12);
        a += b; d ^= a; d = BitMask<Natural32>::barrelShiftMultiply(d, 8 );
        c += d; b ^= c; b = BitMask<Natural32>::barrelShiftMultiply(b, 7 );
    }

    void scramble(NativeNaturalType rounds = 10) {
        for(NativeNaturalType i = 0; i < rounds; ++i) {
            quarterRound(block[0], block[4], block[ 8], block[12]);
            quarterRound(block[1], block[5], block[ 9], block[13]);
            quarterRound(block[2], block[6], block[10], block[14]);
            quarterRound(block[3], block[7], block[11], block[15]);
            quarterRound(block[0], block[5], block[10], block[15]);
            quarterRound(block[1], block[6], block[11], block[12]);
            quarterRound(block[2], block[7], block[ 8], block[13]);
            quarterRound(block[3], block[4], block[ 9], block[14]);
        }
    }

    void generate(ChaCha20& context) {
        for(Natural8 j = 0; j < 16; ++j)
            block[j] = context.block[j];
        scramble();
        for(Natural8 j = 0; j < 16; ++j)
            block[j] += context.block[j];
        ++counter;
    }
};
