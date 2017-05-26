template<bool _value>
struct BoolConstant {
    static constexpr bool value = _value;
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
/*typedef unsigned __int128 Natural128;
typedef __int128 Integer128;
typedef long double Float128;*/

const Natural8 architectureSize = sizeof(void*)*8;
#if __BYTE_ORDER != __LITTLE_ENDIAN
#error Must be compiled with little endian
#endif
#ifdef __LP64__
static_assert(architectureSize == 64);
#else
static_assert(architectureSize == 32);
#endif
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

template<typename Type>
struct sizeOfInBits {
    static constexpr NativeNaturalType value = sizeof(Type)*8;
};
template<>
struct sizeOfInBits<VoidType> {
    static constexpr NativeNaturalType value = 0;
};

constexpr NativeNaturalType architecturePadding(NativeNaturalType bits) {
    return (bits+architectureSize-1)/architectureSize*architectureSize;
}
