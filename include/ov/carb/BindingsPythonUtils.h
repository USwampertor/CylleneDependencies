// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include "BindingsUtils.h"
#include "IObject.h"

#include "cpp17/TypeTraits.h"
#include "cpp17/Functional.h"

// Python uses these in modsupport.h, so undefine them now
#pragma push_macro("min")
#undef min
#pragma push_macro("max")
#undef max

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#pragma pop_macro("min")
#pragma pop_macro("max")

namespace py = pybind11;

PYBIND11_DECLARE_HOLDER_TYPE(T, carb::ObjectPtr<T>, true);

// Provide simple implementations of types used in multiple bindings.
namespace carb
{

template <typename InterfaceType, typename ReturnType, typename... Args>
auto wrapInterfaceFunctionReleaseGIL(ReturnType (*InterfaceType::*p)(Args...))
    -> std::function<ReturnType(InterfaceType&, Args...)>
{
    return [p](InterfaceType& c, Args... args) {
        py::gil_scoped_release g;
        return (c.*p)(args...);
    };
}

template <typename InterfaceType, typename ReturnType, typename... Args>
auto wrapInterfaceFunctionReleaseGIL(const InterfaceType* c, ReturnType (*InterfaceType::*p)(Args...))
    -> std::function<ReturnType(Args...)>
{
    return [c, p](Args... args) {
        py::gil_scoped_release g;
        return (c->*p)(args...);
    };
}

template <typename InterfaceType, typename... PyClassArgs>
py::class_<InterfaceType, PyClassArgs...> defineInterfaceClass(py::module& m,
                                                               const char* className,
                                                               const char* acquireFuncName,
                                                               const char* releaseFuncName = nullptr,
                                                               const char* classDocstring = nullptr)
{
    auto cls = classDocstring ? py::class_<InterfaceType, PyClassArgs...>(m, className, classDocstring) :
                                py::class_<InterfaceType, PyClassArgs...>(m, className);

    m.def(acquireFuncName,
          [](const char* pluginName, const char* libraryPath) {
              return libraryPath ? acquireInterfaceFromLibraryForBindings<InterfaceType>(libraryPath) :
                                   acquireInterfaceForBindings<InterfaceType>(pluginName);
          },
          py::arg("plugin_name") = nullptr, py::arg("library_path") = nullptr, py::return_value_policy::reference);

    if (releaseFuncName)
    {
        m.def(releaseFuncName, [](InterfaceType* iface) { carb::getFramework()->releaseInterface(iface); });
    }

    return cls;
}


/**
 * Assuming std::function will call into python code this function makes it safe.
 * It wraps it into try/catch, acquires GIL lock and log errors.
 */
template <typename Sig, typename... ArgsT>
auto callPythonCodeSafe(const std::function<Sig>& fn, ArgsT&&... args)
{
    using ReturnT = cpp17::invoke_result_t<decltype(fn), ArgsT...>;
    try
    {
        if (fn)
        {
            py::gil_scoped_acquire gilLock;
            return fn(std::forward<ArgsT>(args)...);
        }
    }
    catch (const py::error_already_set& e)
    {
        CARB_LOG_ERROR("%s", e.what());
    }
    catch (const std::runtime_error& e)
    {
        CARB_LOG_ERROR("%s", e.what());
    }

    return ReturnT();
}


/**
 * Helper class implement scripting callbacks.
 * It extends ScriptCallbackRegistry to provide facility to make safe calls of python callback. It adds GIL lock and
 * error handling. ScriptCallbackRegistryPython::call can be passed into C API as C function, as long as FuncT* is
 * passed into as userData.
 */
template <class KeyT, typename ReturnT, typename... Args>
class ScriptCallbackRegistryPython : public ScriptCallbackRegistry<KeyT, ReturnT, Args...>
{
public:
    using typename ScriptCallbackRegistry<KeyT, ReturnT, Args...>::FuncT;

    static ReturnT call(Args... args, void* userData)
    {
        return callTyped((FuncT*)userData, std::forward<Args>(args)...);
    }

    static ReturnT callTyped(FuncT* f, Args&&... args)
    {
        return callPythonCodeSafe(*f, std::forward<Args>(args)...);
    }
};


/**
 * Holds subscription for python in RAII way. Unsubscribe function is called when destroyed.
 */
class Subscription
{
public:
    template <class Unsubscribe>
    explicit Subscription(Unsubscribe&& unsubscribe) : m_unsubscribeFn(std::forward<Unsubscribe>(unsubscribe))
    {
    }

    void unsubscribe()
    {
        if (m_unsubscribeFn)
        {
            m_unsubscribeFn();
            m_unsubscribeFn = nullptr;
        }
    }

    ~Subscription()
    {
        unsubscribe();
    }

private:
    std::function<void()> m_unsubscribeFn;
};

template <class Ret, class... Args>
class PyAdapter
{
    using Function = std::function<Ret(Args...)>;
    Function m_func;

    struct ScopedDestroy
    {
        PyAdapter* m_callable;
        ScopedDestroy(PyAdapter* callable) : m_callable(callable)
        {
        }
        ~ScopedDestroy()
        {
            delete m_callable;
        }
    };

public:
    PyAdapter(Function&& func) : m_func(std::move(func))
    {
    }

    template <class... Args2>
    auto call(Args2&&... args)
    {
        using ReturnType = cpp17::invoke_result_t<Function, Args2...>;
        try
        {
            py::gil_scoped_acquire gil;
            if (m_func)
            {
                return cpp17::invoke(std::move(m_func), std::forward<Args2>(args)...);
            }
        }
        catch (const py::error_already_set& e)
        {
            CARB_LOG_ERROR("%s", e.what());
        }
        catch (const std::runtime_error& e)
        {
            CARB_LOG_ERROR("%s", e.what());
        }
        py::gil_scoped_acquire gil; // Hold the GIL while constructing whatever return type
        return ReturnType();
    }

    // Direct adapter to Carbonite callback when userData is the last argument, the PyAdapter* is the userdata, and
    // multiple calls to this adapter are desired. The adapter must be deleted with `delete` or `destroy()` later.
    static auto adaptCallAndKeep(Args... args, void* user)
    {
        return static_cast<PyAdapter*>(user)->call(std::forward<Args>(args)...);
    }

    // Direct adapter to Carbonite callback when userData is the last argument, the PyAdapter* is the userdata, and
    // there will be only one call to the adapter.
    static auto adaptCallAndDestroy(Args... args, void* user)
    {
        PyAdapter* callable = static_cast<PyAdapter*>(user);
        ScopedDestroy scopedDestroy(callable);
        return callable->call(std::forward<Args>(args)...);
    }

    // Call the adapter with perfect forwarding and keep the adapter around for future calls.
    template <class... Args2>
    static auto callAndKeep(void* user, Args2&&... args)
    {
        return static_cast<PyAdapter*>(user)->call(std::forward<Args2>(args)...);
    }

    // Call the adapter with perfect forwarding and destroy the adapter.
    template <class... Args2>
    static auto callAndDestroy(void* user, Args2&&... args)
    {
        PyAdapter* callable = static_cast<PyAdapter*>(user);
        ScopedDestroy scopedDestroy(callable);
        return callable->call(std::forward<Args2>(args)...);
    }

    static void destroy(void* user)
    {
        delete static_cast<PyAdapter*>(user);
    }
};

template <class Ret, class... Args>
std::unique_ptr<PyAdapter<Ret, Args...>> createPyAdapter(std::function<Ret(Args...)>&& func)
{
    return std::make_unique<PyAdapter<Ret, Args...>>(std::move(func));
}

template <class Callback, class Subscribe, class Unsubscribe>
std::shared_ptr<Subscription> createPySubscription(Callback&& func, Subscribe&& subscribe, Unsubscribe&& unsub)
{
    auto callable = createPyAdapter(std::forward<Callback>(func));
    using Callable = typename decltype(callable)::element_type;
    auto&& id = subscribe(Callable::adaptCallAndKeep, callable.get());

    return std::make_shared<Subscription>(
        [unsub = std::forward<Unsubscribe>(unsub), id = std::move(id), callable = callable.release()] {
            unsub(id);
            delete callable;
        });
}

/**
 * Set of helpers to pass std::function (from python bindings) in Carbonite interfaces.
 * Deprecated: use PyAdapter instead via createPyAdapter()/createPySubscription()
 */
template <typename ReturnT, typename... ArgsT>
class FuncUtils
{
public:
    using StdFuncT = std::function<ReturnT(ArgsT...)>;
    using CallbackT = ReturnT (*)(ArgsT..., void*);


    static ReturnT callPythonCodeSafe(const std::function<ReturnT(ArgsT...)>& fn, ArgsT... args)
    {
        return carb::callPythonCodeSafe(fn, args...);
    }

    static ReturnT callbackWithUserData(ArgsT... args, void* userData)
    {
        StdFuncT* fn = (StdFuncT*)userData;
        if (fn)
            return callPythonCodeSafe(*fn, args...);
        else
            return ReturnT();
    }

    static StdFuncT* createStdFuncCopy(const StdFuncT& fn)
    {
        return new StdFuncT(fn);
    }

    static void destroyStdFuncCopy(StdFuncT* fn)
    {
        delete fn;
    }

    /**
     * If you have std::function which calls into python code and an interface with pair of subscribe/unsubscribe
     * functions, this function:
     *     1. Prolong lifetime of std::function (and thus python callable) by making copy of it on heap.
     *     2. Subscribes to interface C-style subscribe function by passing this std::function as void* userData (and
     * calling it back safely)
     *     3. Wraps subscription id into Subscription class returned to python. Which holds subscription and
     * automatically unsubscribes when dead.
     */
    template <class SubscriptionT>
    static std::shared_ptr<Subscription> buildSubscription(const StdFuncT& fn,
                                                           SubscriptionT (*subscribeFn)(CallbackT, void*),
                                                           void (*unsubscribeFn)(SubscriptionT))
    {
        StdFuncT* funcCopy = new StdFuncT(fn);
        auto id = subscribeFn(callbackWithUserData, funcCopy);

        auto subscription = std::make_shared<Subscription>([=]() {
            unsubscribeFn(id);
            delete funcCopy;
        });
        return subscription;
    }
};

template <class T>
struct StdFuncUtils;

template <class R, class... Args>
struct StdFuncUtils<std::function<R(Args...)>> : public FuncUtils<R, Args...>
{
};

template <class R, class... Args>
struct StdFuncUtils<const std::function<R(Args...)>> : public FuncUtils<R, Args...>
{
};

template <class R, class... Args>
struct StdFuncUtils<const std::function<R(Args...)>&> : public FuncUtils<R, Args...>
{
};

/**
 * Helper to wrap function that returns `IObject*` into the same function that returns stolen ObjectPtr<IObject> holder
 */
template <typename ReturnT, typename... Args>
std::function<ReturnT(Args...)> wrapPythonCallback(std::function<ReturnT(Args...)>&& c)
{
    return [c = std::move(c)](Args... args) -> ReturnT { return callPythonCodeSafe(c, std::forward<Args>(args)...); };
}

} // namespace carb


#ifdef DOXYGEN_BUILD
/**
 * Macro that allows disabling pybind's use of RTTI to perform duck typing.
 *
 * Given a pointer, pybind uses RTTI to figure out the actual type of the pointer (e.g. given an `IObject*`, RTTI can be
 * used to figure out the pointer is really an `IWindow*`). once pybind knows the "real" type, is generates a PyObject
 * that contains wrappers for all of the "real" types methods.
 *
 * Unfortunately, RTTI is compiler dependent (not @rstref{ABI-safe <abi-compatibility>}) and we've disabled it in much
 * of our code.
 *
 * The `polymorphic_type_hook` specializations generated by this macro disables pybind from using RTTI to find the
 * "real" type of a pointer.  this mean that when using our bindings in Python, you have to "cast" objects to access a
 * given interface. For example:
 * ```python
 *   obj = func_that_returns_iobject()
 *   win = IWindow(obj) # a cast. None is returned if the cast fails.
 *   if win:
 *     win->title = "hi"
 * ```
 *
 * As an aside, since implementations can implement multiple interfaces and the actual implementations are hidden to
 * pybind (we create bindings for interfaces not implementations), the pybind "duck" typing approach was never going to
 * work for us. Said differently, some sort of "cast to this interface" was inevitable.
 * @param TYPE The type to disable Pythonic dynamic casting for.
 */
#    define DISABLE_PYBIND11_DYNAMIC_CAST(TYPE)
#else
#    define DISABLE_PYBIND11_DYNAMIC_CAST(TYPE)                                                                        \
        namespace pybind11                                                                                             \
        {                                                                                                              \
        template <>                                                                                                    \
        struct polymorphic_type_hook<TYPE>                                                                             \
        {                                                                                                              \
            static const void* get(const TYPE* src, const std::type_info*&)                                            \
            {                                                                                                          \
                return src;                                                                                            \
            }                                                                                                          \
        };                                                                                                             \
        template <typename itype>                                                                                      \
        struct polymorphic_type_hook<                                                                                  \
            itype,                                                                                                     \
            detail::enable_if_t<std::is_base_of<TYPE, itype>::value && !std::is_same<TYPE, itype>::value>>             \
        {                                                                                                              \
            static const void* get(const TYPE* src, const std::type_info*&)                                            \
            {                                                                                                          \
                return src;                                                                                            \
            }                                                                                                          \
        };                                                                                                             \
        }
#endif

DISABLE_PYBIND11_DYNAMIC_CAST(carb::IObject)
