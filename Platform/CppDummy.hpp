#include <new>
#include <typeinfo>
#include <setjmp.h>
#include <stdlib.h> // TODO: Remove malloc and free
#include <stdio.h>

[[ noreturn ]] void crash(const char* message) {
    perror(message);
    _exit(1);
}

#undef assert
#define tokenToString(x) #x
#define macroToString(x) tokenToString(x)
#define assert(condition) if(!(condition)) { \
    perror("Assertion failed in " __FILE__ ":" macroToString(__LINE__)); \
    _exit(1); \
}

void* operator new(size_t size) {
    crash("operator new called");
}

void operator delete(void* ptr) noexcept {
    crash("operator delete called");
}

namespace std {
    type_info::~type_info() { }
}

#define DummyTypeInfo(Child, Parent) struct Child : public Parent { virtual ~Child(); }; Child::~Child() { }

namespace __cxxabiv1 {

    DummyTypeInfo(__shim_type_info, std::type_info)
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

        __attribute__((noreturn))
        void __cxa_pure_virtual(void) {
            crash("pure virtual function called");
        }

        __attribute__((noreturn))
        void __cxa_deleted_virtual(void) {
            crash("deleted virtual function called");
        }

        /*void* __dynamic_cast(const void* srcPtr, const __class_type_info* srcType, const __class_type_info* dstType, ptrdiff_t src2dstOffset) {
            printf("dynamic_cast");
            abort();
        }*/

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

template<typename ReturnType, typename... Arguments>
struct CallableContainer {
    virtual ReturnType operator()(Arguments...) = 0;
};

template<typename LambdaType, typename ReturnType, typename... Arguments>
struct LambdaContainer : public CallableContainer<ReturnType, Arguments...> {
    LambdaType* lambda;
    LambdaContainer(LambdaType& _lambda) :lambda(&_lambda) {}
    ReturnType operator()(Arguments... arguments) {
        return (*lambda)(arguments...);
    }
};

template<typename ReturnType, typename... Arguments>
struct Closure {
    void* payload[2];
    template<typename LambdaType>
    Closure(LambdaType&& lambda) {
        ::new(&payload) LambdaContainer<LambdaType, ReturnType, Arguments...>(lambda);
    }
    Closure(decltype(nullptr)) :payload{0, 0} { }
    Closure(Closure& other) :payload{other.payload[0], other.payload[1]} { }
    operator bool() const {
        return payload[1];
    }
    ReturnType operator()(Arguments... arguments) {
        if(!*this)
            crash("empty closure called");
        return reinterpret_cast<CallableContainer<ReturnType, Arguments...>&>(payload)(arguments...);
    }
};

typedef uint64_t ArchitectureType;
typedef ArchitectureType PageRefType;
const ArchitectureType ArchitectureSize = sizeof(ArchitectureType)*8;

constexpr ArchitectureType architecturePadding(ArchitectureType bits) {
    return (bits+ArchitectureSize-1)/ArchitectureSize*ArchitectureSize;
}

ArchitectureType strlen(const char* str) {
    const char* pos;
    for(pos = str; *pos; ++pos);
    return pos-str;
}

int memcmp(const void* a, const void* b, ArchitectureType length) {
    const uint8_t *posA = reinterpret_cast<const uint8_t*>(a),
                  *posB = reinterpret_cast<const uint8_t*>(b),
                  *end = posA+length;
    while(posA < end) {
        if(*posA != *posB)
            return *posA-*posB;
        ++posA;
        ++posB;
    }
    return 0;
}

bool stringEndsWith(const char* str, const char* end) {
    ArchitectureType endLen = strlen(end);
    return memcmp(str+strlen(str)-endLen, end, endLen) == 0;
}
