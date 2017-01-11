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

struct MersenneTwister64 {
    const static Natural64
        mag01 = 0xB5026F5AA96619E9ULL,
        LM = BitMask<Natural64>::fillLSBs(31),
        UM = BitMask<Natural64>::fillMSBs(33);
    const static Natural16 NN = 312, MM = 156;
    Natural64 mt[NN];
    Natural16 mti;

    void reset(Natural64 seed) {
        mt[0] = seed;
        for(mti = 1; mti < NN; ++mti)
            mt[mti] = mti+0x5851F42D4C957F2DULL*(mt[mti-1]^(mt[mti-1]>>62));
    }

    Natural64 operator()() {
        Natural16 i;
        Natural64 x;
        if(mti >= NN) {
            mti = 0;
            for(i = 0; i < NN-MM; ++i) {
                x = (mt[i]&UM)|(mt[i+1]&LM);
                mt[i] = mt[i+MM] ^ (x>>1) ^ (mag01*(x&1ULL));
            }
            for(; i < NN-1; ++i) {
                x = (mt[i]&UM)|(mt[i+1]&LM);
                mt[i] = mt[i+(MM-NN)] ^ (x>>1) ^ (mag01*(x&1ULL));
            }
            x = (mt[NN-1]&UM)|(mt[0]&LM);
            mt[NN-1] ^= (x>>1) ^ (mag01*(x&1ULL));
        }
        x = mt[mti++];
        x ^= (x>>29)&0x5555555555555555ULL;
        x ^= (x<<17)&0x71D67FFFEDA60000ULL;
        x ^= (x<<37)&0xFFF7EEE000000000ULL;
        x ^= (x>>43);
        return x;
    }
};
