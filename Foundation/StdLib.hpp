#include <Foundation/DataTypes.hpp>

#define IMPORT extern
#define EXPORT __attribute__((visibility("default")))
#define DO_NOT_INLINE __attribute__((noinline))
#define tokenToString(x) #x
#define macroToString(x) tokenToString(x)
#define assert(condition) { \
    if(!__builtin_expect(static_cast<bool>(condition), 0)) \
        assertFailed(__FILE__ ":" macroToString(__LINE__)); \
}

extern "C" {
    void assertFailed(const char* message);
    NativeNaturalType strlen(const char* str) {
        const char* pos;
        for(pos = str; *pos; ++pos);
        return pos-str;
    }
    void* memcpy(void* dst, const void* src, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            reinterpret_cast<char*>(dst)[i] = reinterpret_cast<const char*>(src)[i];
        return dst;
    }
    void* memset(void* dst, NativeNaturalType value, NativeNaturalType len) {
        for(NativeNaturalType i = 0; i < len; ++i)
            reinterpret_cast<char*>(dst)[i] = value;
        return dst;
    }
    void __cxa_atexit(void(*)(void*), void*, void*) {}
    void __cxa_pure_virtual() {}
    void __cxa_deleted_virtual() {}
}

inline void* operator new(__SIZE_TYPE__, void* ptr) noexcept {
    return ptr;
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

struct Ascending {
    template<typename KeyType>
    static bool compare(KeyType a, KeyType b) {
        return a < b;
    }
};

struct Descending {
    template<typename KeyType>
    static bool compare(KeyType a, KeyType b) {
        return a > b;
    }
};

struct Equal {
    template<typename KeyType>
    static bool compare(KeyType a, KeyType b) {
        return a == b;
    }
};

template<typename _FirstType, typename _SecondType = VoidType>
struct Pair {
    typedef _FirstType FirstType;
    typedef _SecondType SecondType;
    FirstType first;
    SecondType second;
    Pair() {}
    Pair(FirstType _first) :first(_first) {}
    Pair(FirstType _first, SecondType _second) :first(_first), second(_second) {}
    operator FirstType() {
        return first;
    }
};

template<typename FunctionType>
struct CallableContainer;

template<typename ReturnType, typename... Arguments>
struct CallableContainer<ReturnType(Arguments...)> {
    typedef ReturnType(*InvocationPtr)(void const*, Arguments...);
    void const* lambda;
    InvocationPtr invocationPtr;
    CallableContainer() :lambda(nullptr), invocationPtr(nullptr) {}
    CallableContainer(void const* _lambda, InvocationPtr _invocationPtr) :lambda(_lambda), invocationPtr(_invocationPtr) {}
    inline ReturnType operator()(Arguments... arguments) {
        return invocationPtr(lambda, arguments...);
    }
};

template<typename LambdaType, typename ReturnType, typename... Arguments>
struct LambdaContainer : public CallableContainer<ReturnType(Arguments...)> {
    LambdaContainer(LambdaType const* _lambda) :CallableContainer<ReturnType(Arguments...)>(_lambda, &invoke) {}
    static ReturnType invoke(void const* lambda, Arguments... arguments) {
        return (*reinterpret_cast<LambdaType const*>(lambda))(arguments...);
    }
};

template<typename FunctionType>
struct Closure;

template<typename ReturnType, typename... Arguments>
struct Closure<ReturnType(Arguments...)> {
    CallableContainer<ReturnType(Arguments...)> payload;
    template<typename LambdaType>
    Closure(LambdaType const& lambda) {
        ::new(&payload) LambdaContainer<LambdaType, ReturnType, Arguments...>(&lambda);
    }
    Closure(decltype(nullptr)) {}
    inline operator bool() const {
        return payload.lambda;
    }
    inline ReturnType operator()(Arguments... arguments) {
        assert(*this);
        return reinterpret_cast<CallableContainer<ReturnType(Arguments...)>&>(payload)(arguments...);
    }
};

template<typename IndexType>
IndexType binarySearch(IndexType begin, IndexType end, Closure<bool(IndexType)> compare) {
    while(begin < end) {
        IndexType mid = (begin+end)/2;
        if(compare(mid))
            begin = mid+1;
        else
            end = mid;
    }
    return begin;
}
