#include <setjmp.h>
#include <stdio.h> // TODO: Remove assert
#include <stdlib.h> // TODO: Remove malloc and free

#undef assert
#define tokenToString(x) #x
#define macroToString(x) tokenToString(x)
#define assert(condition) if(!(condition)) { \
    perror("Assertion failed in " __FILE__ ":" macroToString(__LINE__)); \
    exit(1); \
}

void operator delete(void* ptr) noexcept {}
inline void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}

template<bool _value>
struct BoolConstant {
    static const bool value = _value;
};
template<typename type, typename otherType>
struct isSame : public BoolConstant<false> {};
template<typename type>
struct isSame<type, type> : public BoolConstant<true> {};

template<bool B, class T = void>
struct enableIf {};
template<class T>
struct enableIf<true, T> {
    typedef T type;
};

template<bool B, class T, class F>
struct conditional {
    typedef F type;
};
template<class T, class F>
struct conditional<true, T, F> {
    typedef T type;
};

namespace __cxxabiv1 {

#define DummyTypeInfo(Child, Parent) struct Child : public Parent { virtual ~Child(); }; Child::~Child() {}

    struct type_info {};
    DummyTypeInfo(__shim_type_info, type_info)
    DummyTypeInfo(__fundamental_type_info, __shim_type_info)
    DummyTypeInfo(__array_type_info, __shim_type_info)
    DummyTypeInfo(__function_type_info, __shim_type_info)
    DummyTypeInfo(__enum_type_info, __shim_type_info)
    DummyTypeInfo(__class_type_info, __shim_type_info)
    DummyTypeInfo(__si_class_type_info, __class_type_info)
    DummyTypeInfo(__vmi_class_type_info, __class_type_info)
    DummyTypeInfo(__pbase_type_info, __shim_type_info)
    DummyTypeInfo(__pointer_type_info, __pbase_type_info)
    DummyTypeInfo(__pointer_to_member_type_info, __pbase_type_info)

    extern "C" {
        void __cxa_pure_virtual(void) {}
        void __cxa_deleted_virtual(void) {}
    }
}

template <typename T>
T min(T a, T b) {
    return (a < b) ? a : b;
}

template<typename T, typename... Args>
T min(T c, Args... args) {
    return min(c, min(args...));
}

template <typename T>
T max(T a, T b) {
    return (a > b) ? a : b;
}

template<typename T, typename... Args>
T max(T c, Args... args) {
    return max(c, max(args...));
}

template<typename FunctionType>
struct CallableContainer;

template<typename ReturnType, typename... Arguments>
struct CallableContainer<ReturnType(Arguments...)> {
    virtual ReturnType operator()(Arguments...) = 0;
};

template<typename LambdaType, typename ReturnType, typename... Arguments>
struct LambdaContainer : public CallableContainer<ReturnType(Arguments...)> {
    LambdaType* lambda;
    LambdaContainer(LambdaType& _lambda) :lambda(&_lambda) {}
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
    Closure(LambdaType&& lambda) {
        ::new(&payload) LambdaContainer<LambdaType, ReturnType, Arguments...>(lambda);
    }
    Closure(decltype(nullptr)) :payload{0, 0} {}
    Closure(Closure& other) :payload{other.payload[0], other.payload[1]} {}
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

typedef long long unsigned int NativeNaturalType;
typedef long long int NativeIntegerType;
typedef double NativeFloatType;
typedef NativeNaturalType PageRefType;
typedef NativeNaturalType Symbol;
const NativeNaturalType architectureSize = sizeof(NativeNaturalType)*8;
struct VoidType {} VoidValue;

constexpr NativeNaturalType architecturePadding(NativeNaturalType bits) {
    return (bits+architectureSize-1)/architectureSize*architectureSize;
}

template<typename DataType>
struct BitMask {
    const static NativeNaturalType bits = sizeof(DataType)*8;
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
constexpr NativeNaturalType BitMask<unsigned long>::clz(unsigned long value) {
    return __builtin_clzl(value);
}

template<>
constexpr NativeNaturalType BitMask<unsigned long>::ctz(unsigned long value) {
    return __builtin_ctzl(value);
}

template<>
constexpr NativeNaturalType BitMask<unsigned long long>::clz(unsigned long long value) {
    return __builtin_clzll(value);
}

template<>
constexpr NativeNaturalType BitMask<unsigned long long>::ctz(unsigned long long value) {
    return __builtin_ctzll(value);
}

NativeNaturalType strlen(const char* str) {
    const char* pos;
    for(pos = str; *pos; ++pos);
    return pos-str;
}
