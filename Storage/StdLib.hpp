template<bool _value>
struct BoolConstant {
    static const bool value = _value;
};
template<typename type, typename otherType>
struct isSame : public BoolConstant<false> {};
template<typename type>
struct isSame<type, type> : public BoolConstant<true> {};

template<bool B, typename T, typename F>
struct conditional {
    typedef F type;
};
template<typename T, typename F>
struct conditional<true, T, F> {
    typedef T type;
};

typedef char unsigned Natural8;
typedef char Integer8;
typedef short unsigned Natural16;
typedef short Integer16;
typedef unsigned Natural32;
typedef int Integer32;
typedef float Float32;
typedef long long unsigned Natural64;
typedef long long int Integer64;
typedef double Float64;
// typedef unsigned __int128 Natural128;
// typedef __int128 Integer128;
// typedef long double Float128;

const Natural8 architectureSize = sizeof(void*)*8;
typedef conditional<architectureSize == 32, Natural32, Natural64>::type NativeNaturalType;
typedef conditional<architectureSize == 32, Integer32, Integer64>::type NativeIntegerType;
typedef conditional<architectureSize == 32, Float32, Float64>::type NativeFloatType;
typedef NativeNaturalType PageRefType;
typedef NativeNaturalType Symbol;

struct VoidType {
    VoidType() {}
    template<typename DataType>
    VoidType(DataType) {}
    template<typename DataType>
    operator DataType() {
        return 0;
    }
    VoidType& operator+=(VoidType) {
        return *this;
    }
    VoidType operator-(VoidType) {
        return VoidType();
    }
    bool operator>(VoidType) {
        return false;
    }
    bool operator>=(VoidType) {
        return false;
    }
    bool operator==(VoidType) {
        return false;
    }
};

constexpr NativeNaturalType architecturePadding(NativeNaturalType bits) {
    return (bits+architectureSize-1)/architectureSize*architectureSize;
}

template<typename Type>
struct sizeOfInBits {
    static constexpr NativeNaturalType value = sizeof(Type)*8;
};
template<>
struct sizeOfInBits<VoidType> {
    static constexpr NativeNaturalType value = 0;
};

extern "C" {
#define EXPORT __attribute__((visibility("default")))
#define DO_NOT_INLINE __attribute__((noinline))
#define tokenToString(x) #x
#define macroToString(x) tokenToString(x)
#define assert(condition) { \
        if(!(condition)) \
            assertFailed("Assertion failed in " __FILE__ ":" macroToString(__LINE__)); \
    }
    void assertFailed(const char* str);
    NativeNaturalType strlen(const char* str) {
        const char* pos;
        for(pos = str; *pos; ++pos);
        return pos-str;
    }
    NativeNaturalType memcpy(void* dst, void* src, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            reinterpret_cast<char*>(dst)[i] = reinterpret_cast<char*>(src)[i];
        return 0;
    }
    NativeNaturalType memset(void* dst, NativeNaturalType value, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            reinterpret_cast<char*>(dst)[i] = value;
        return 0;
    }
    void __cxa_atexit(void(*)(void*), void*, void*) {}
    void __cxa_pure_virtual() {}
    void __cxa_deleted_virtual() {}
}

inline void* operator new(conditional<architectureSize == 32, NativeNaturalType, unsigned long>::type, void* ptr) noexcept {
    return ptr;
}

template<typename FunctionType>
struct CallableContainer;

template<typename ReturnType, typename... Arguments>
struct CallableContainer<ReturnType(Arguments...)> {
    virtual ReturnType operator()(Arguments...) = 0;
};

template<typename LambdaType, typename ReturnType, typename... Arguments>
struct LambdaContainer : public CallableContainer<ReturnType(Arguments...)> {
    LambdaType const* lambda;
    LambdaContainer(LambdaType const* _lambda) :lambda(_lambda) {}
    ReturnType operator()(Arguments... arguments) {
        return (*lambda)(arguments...);
    }
};

template<typename FunctionType>
struct Closure;

template<typename ReturnType, typename... Arguments>
struct Closure<ReturnType(Arguments...)> {
    void* payload[2];
    template<typename LambdaType>
    Closure(LambdaType const& lambda) {
        ::new(&payload) LambdaContainer<LambdaType, ReturnType, Arguments...>(&lambda);
    }
    Closure(decltype(nullptr)) :payload{0, 0} {}
    operator bool() const {
        return payload[1];
    }
    ReturnType operator()(Arguments... arguments) {
        assert(*this);
        return reinterpret_cast<CallableContainer<ReturnType(Arguments...)>&>(payload)(arguments...);
    }
};

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

template<typename T>
constexpr T min(T a, T b) {
    return (a < b) ? a : b;
}

template<typename T, typename... Args>
constexpr T min(T c, Args... args) {
    return min(c, min(args...));
}

template<typename T>
constexpr T max(T a, T b) {
    return (a > b) ? a : b;
}

template<typename T, typename... Args>
constexpr T max(T c, Args... args) {
    return max(c, max(args...));
}

template<typename DataType>
struct BitMask {
    const static NativeNaturalType bits = sizeOfInBits<DataType>::value;
    const static DataType empty = 0, one = 1, full = ~empty;
    constexpr static DataType fillLSBs(NativeNaturalType len) {
        return (len == bits) ? full : (one<<len)-one;
    }
    constexpr static DataType fillMSBs(NativeNaturalType len) {
        return (len == 0) ? empty : ~((one<<(bits-len))-one);
    }
    constexpr static NativeNaturalType clz(DataType value);
    constexpr static NativeNaturalType ctz(DataType value);
    constexpr static NativeNaturalType ceilLog2(DataType value) {
        return bits-clz(value);
    }
};

template<>
constexpr NativeNaturalType BitMask<Natural32>::clz(Natural32 value) {
    return __builtin_clzl(value);
}

template<>
constexpr NativeNaturalType BitMask<Natural32>::ctz(Natural32 value) {
    return __builtin_ctzl(value);
}

template<>
constexpr NativeNaturalType BitMask<Natural64>::clz(Natural64 value) {
    return __builtin_clzll(value);
}

template<>
constexpr NativeNaturalType BitMask<Natural64>::ctz(Natural64 value) {
    return __builtin_ctzll(value);
}

template<typename DataType>
constexpr static DataType swapedEndian(DataType value);

template<>
constexpr Natural16 swapedEndian(Natural16 value) {
    return __builtin_bswap16(value);
}

template<>
constexpr Natural32 swapedEndian(Natural32 value) {
    return __builtin_bswap32(value);
}

template<>
constexpr Natural64 swapedEndian(Natural64 value) {
    return __builtin_bswap64(value);
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
