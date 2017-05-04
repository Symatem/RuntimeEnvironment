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
    const char* gitRef = "git:" macroToString(GIT_REF);
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
