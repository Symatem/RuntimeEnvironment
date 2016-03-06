//__attribute__((__weak__, __visibility__("default")))
void* operator new(std::size_t size) {
    void* ptr = malloc(size);
    printf("operator new %zu %p\n", size, ptr);
    // abort();
    return ptr;
}

//__attribute__((__weak__, __visibility__("default")))
void operator delete(void* ptr) noexcept {
    printf("operator delete %p\n", ptr);
    // abort();
    free(ptr);
}

/*__attribute__((__weak__, __visibility__("default")))
void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return ::operator new(size);
}

__attribute__((__weak__, __visibility__("default")))
void* operator new[](size_t size) {
    return ::operator new(size);
}

__attribute__((__weak__, __visibility__("default")))
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return ::operator new[](size);
}*/

/*__attribute__((__weak__, __visibility__("default")))
void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    ::operator delete(ptr);
}

__attribute__((__weak__, __visibility__("default")))
void operator delete[] (void* ptr) noexcept {
    ::operator delete(ptr);
}

__attribute__((__weak__, __visibility__("default")))
void operator delete[] (void* ptr, const std::nothrow_t&) noexcept {
    ::operator delete[](ptr);
}*/

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
            printf("Pure virtual function called!");
            abort();
        }

        __attribute__((noreturn))
        void __cxa_deleted_virtual(void) {
            printf("Deleted virtual function called!");
            abort();
        }

        /*void* __dynamic_cast(const void* srcPtr, const __class_type_info* srcType, const __class_type_info* dstType, std::ptrdiff_t src2dstOffset) {
            printf("dynamic_cast");
            abort();
        }*/

    }
}
