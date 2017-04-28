#include <Foundation/StdLib.hpp>

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
