#include <setjmp.h> // TODO: Replace setjmp/longjmp
#include <stdlib.h> // TODO: Remove malloc and free

template<bool _value>
struct BoolConstant {
    static const bool value = _value;
};
template<typename type, typename otherType>
struct isSame : public BoolConstant<false> {};
template<typename type>
struct isSame<type, type> : public BoolConstant<true> {};

template<bool B, class T, class F>
struct conditional {
    typedef F type;
};
template<class T, class F>
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

#ifdef WEB_ASSEMBLY
typedef Natural32 NativeNaturalType;
typedef Integer32 NativeIntegerType;
typedef Float32 NativeFloatType;
#else
typedef Natural64 NativeNaturalType;
typedef Integer64 NativeIntegerType;
typedef Float64 NativeFloatType;
#endif

typedef NativeNaturalType PageRefType;
typedef NativeNaturalType Symbol;
const NativeNaturalType architectureSize = sizeof(NativeNaturalType)*8;

struct VoidType {
    VoidType() {}
    template<typename DataType>
    VoidType(DataType) {}
    template<typename DataType>
    operator DataType() {
        return 0;
    }
    template<typename DataType>
    VoidType& operator+=(DataType) {
        return *this;
    }
    VoidType operator-(VoidType) {
        return VoidType();
    }
};
bool operator>(VoidType, VoidType) {
    return false;
}
bool operator==(VoidType, VoidType) {
    return false;
}
template<typename DataType>
bool operator>=(VoidType, DataType) {
    return false;
}
template<typename DataType>
VoidType operator-(VoidType, DataType) {
    return VoidType();
}
template<typename DataType>
VoidType operator-(DataType, VoidType) {
    return VoidType();
}


template<class Type>
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
    Integer32 atexit(void (*func)()) {
        return 1;
    }
    void __cxa_pure_virtual(void) {}
    void __cxa_deleted_virtual(void) {}
}

namespace __cxxabiv1 {
#define DummyTypeInfo(Child, Parent) \
    struct Child : public Parent { \
        virtual ~Child(); \
    }; \
    Child::~Child() {}
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
}

void operator delete(void* ptr) noexcept {}
inline void* operator new(unsigned long, void* ptr) noexcept {
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

constexpr NativeNaturalType architecturePadding(NativeNaturalType bits) {
    return (bits+architectureSize-1)/architectureSize*architectureSize;
}

NativeNaturalType pointerToNatural(void* ptr) {
    return reinterpret_cast<long unsigned>(ptr);
}

template<typename T>
T min(T a, T b) {
    return (a < b) ? a : b;
}

template<typename T, typename... Args>
T min(T c, Args... args) {
    return min(c, min(args...));
}

template<typename T>
T max(T a, T b) {
    return (a > b) ? a : b;
}

template<typename T, typename... Args>
T max(T c, Args... args) {
    return max(c, max(args...));
}
