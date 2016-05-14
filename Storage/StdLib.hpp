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
    Integer32 atexit(void (*func)()) {
        func = 0;
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

void operator delete(void* ptr) noexcept {
    ptr = 0;
}
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
