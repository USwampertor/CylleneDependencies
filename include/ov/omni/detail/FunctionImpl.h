// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#pragma once

#include "../../carb/Defines.h"
#include "../../carb/Memory.h"
#include "../../carb/cpp17/Functional.h"
#include "../../carb/cpp17/TypeTraits.h"
#include "../../carb/detail/NoexceptType.h"
#include "../core/Assert.h"
#include "../core/IObject.h"
#include "../core/ResultError.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <utility>

namespace omni
{

template <typename Signature>
class function;

//! \cond DEV
namespace detail
{

CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE()

// NOTE: GCC added a warning about the use of memcpy and memset on class types with non-trivial copy-assignment. In most
// cases, this is a valid warning that should be paid attention to.
#if CARB_COMPILER_GNUC && __GNUC__ >= 8
#    define OMNI_FUNCTION_PUSH_IGNORE_CLASS_MEMACCESS() CARB_IGNOREWARNING_GNUC_WITH_PUSH("-Wclass-memaccess")
#    define OMNI_FUNCTION_POP_IGNORE_CLASS_MEMACCESS() CARB_IGNOREWARNING_GNUC_POP
#else
#    define OMNI_FUNCTION_PUSH_IGNORE_CLASS_MEMACCESS()
#    define OMNI_FUNCTION_POP_IGNORE_CLASS_MEMACCESS()
#endif

using carb::cpp17::bool_constant;
using carb::cpp17::conjunction;
using carb::cpp17::disjunction;
using carb::cpp17::is_invocable_r;
using carb::cpp17::negation;
using carb::cpp17::void_t;
using omni::core::Result;

//! Called when attempting to invoke a function that is unbound. It will throw \c std::bad_function_call if exceptions
//! are enabled or terminate immediately.
[[noreturn]] inline void null_function_call()
{
#if CARB_EXCEPTIONS_ENABLED
    throw std::bad_function_call();
#else
    OMNI_FATAL_UNLESS(false, "Attempt to call null function");
    std::terminate(); // Enforce [[noreturn]]
#endif
}

//! Called when an internal function operation returns a non-success error case. This function will throw a
//! \c ResultError if exceptions are enabled or terminate immediately.
[[noreturn]] inline void function_operation_failure(Result rc, char const* msg)
{
#if CARB_EXCEPTIONS_ENABLED
    throw core::ResultError(rc, msg);
#else
    OMNI_FATAL_UNLESS(false, "%s: %s", msg, core::resultToString(rc));
    std::terminate(); // Enforce [[noreturn]]
#endif
}

//! Metafunction to check if \c T is a \c omni::function or not.
template <typename T>
struct IsFunction : std::false_type
{
};

template <typename Signature>
struct IsFunction<omni::function<Signature>> : std::true_type
{
};

constexpr std::size_t kFUNCTION_BUFFER_SIZE = 16U;
constexpr std::size_t kFUNCTION_BUFFER_ALIGN = alignof(std::max_align_t);
constexpr std::size_t kFUNCTION_SIZE = 32U;

//! \see FunctionBuffer
struct FunctionUnusedBase
{
};

//! \see FunctionBuffer
struct FunctionUnused : virtual FunctionUnusedBase
{
};

//! This data type stores the content of the functor or a pointer to it. Only the \c raw member is used by the
//! implementation, but the other union members are here to make sure size and alignment stay as expected even when
//! strange types are used.
union alignas(kFUNCTION_BUFFER_ALIGN) FunctionBuffer
{
    std::array<char, kFUNCTION_BUFFER_SIZE> raw;
    void* pointer;
    void (*pfunc)();
    void (FunctionUnused::*pmemfunc)();
    std::max_align_t FunctionUnused::*pmemsel;
};

static_assert(sizeof(FunctionBuffer) == kFUNCTION_BUFFER_SIZE, "Actual size of FunctionBuffer does not match goal");
static_assert(alignof(FunctionBuffer) == kFUNCTION_BUFFER_ALIGN, "Actual align of FunctionBuffer does not match goal");

struct FunctionCharacteristics
{
    //! The size of this instance. This is useful for ABI compatibility if needs expand.
    size_t _size;

    //! Called on function destructor. If the target functor is trivially-destructible, then this should be \c nullptr
    //! to make nothing happen on destruction.
    void (*destroy)(FunctionBuffer* self);

    //! Called when a function is copied. If the target functor is trivially-relocatable, then this should be \c nullptr
    //! so the buffer is copied by \c memcpy.
    Result (*clone)(FunctionBuffer* target, FunctionBuffer const* source);

    constexpr FunctionCharacteristics(void (*destroy_)(FunctionBuffer*),
                                      Result (*clone_)(FunctionBuffer*, FunctionBuffer const*)) noexcept
        : _size{ sizeof(FunctionCharacteristics) }, destroy{ destroy_ }, clone{ clone_ }
    {
    }
};

//! This holds the core data of the \c omni::function type.
struct FunctionData
{
    FunctionBuffer m_buffer;

    //! The entry point into the function. The real type is `TReturn(*)(FunctionBuffer const*, TArgs...)`.
    void* m_trampoline;

    //! The management of the erased target of this function. This will be null in cases where the copy constructor and
    //! destructor are trivial, meaning \c memcpy and \c memset are acceptable substitutes.
    FunctionCharacteristics const* m_characteristics;

    explicit FunctionData(std::nullptr_t) noexcept
    {
        clear_unsafe();
    }

    FunctionData(FunctionData const& src)
    {
        // NOTE: Another approach could be to copy m_trampoline and m_characteristics, then conditionally memcpy the
        // m_buffer. However, the current approach of unconditional memcpy makes the operation twice as fast.
        copy_from_unsafe(src);

        if (m_characteristics && m_characteristics->clone)
        {
            if (auto rc = m_characteristics->clone(&m_buffer, &src.m_buffer))
            {
                clear_unsafe();
                function_operation_failure(rc, "failed to clone function");
            }
        }
    }

    FunctionData(FunctionData&& src) noexcept
    {
        copy_from_unsafe(src);
        src.clear_unsafe();
    }

    //! Initialize through binding with the given \a assigner and functor \a f.
    //!
    //! \tparam Assigner The assigner type for this function, picked through `FunctionAssigner<F>`.
    template <typename Assigner, typename F>
    FunctionData(Assigner assigner, F&& f)
    {
        if (auto rc = bind_unsafe(assigner, std::forward<F>(f)))
        {
            function_operation_failure(rc, "failed to bind to functor");
        }
    }

    FunctionData& operator=(FunctionData const& src)
    {
        if (this == &src)
            return *this;

        // The copy constructor can throw, but the move-assignment can not, so we get the proper commit or rollback
        // semantics
        return operator=(FunctionData{ src });
    }

    FunctionData& operator=(FunctionData&& src) noexcept
    {
        if (this == &src)
            return *this;

        reset();
        copy_from_unsafe(src);
        src.clear_unsafe();
        return *this;
    }

    ~FunctionData() noexcept
    {
        reset();
    }

    //! Clear the contents of this instance without checking.
    //!
    //! \pre No cleanup is performed, so the responsibility for doing this must have been taken care of (`reset`) or
    //!      deferred (move-assigned from).
    void clear_unsafe() noexcept
    {
        OMNI_FUNCTION_PUSH_IGNORE_CLASS_MEMACCESS()
        std::memset(this, 0, sizeof(*this));
        OMNI_FUNCTION_POP_IGNORE_CLASS_MEMACCESS()
    }

    //! Copy the contents of \a src to this instance without clearing.
    //!
    //! \pre `this != &src`
    //! \pre No cleanup is performed, so the responsibility for doing so must have been taken care of (`reset`) or
    //!      deferred (copied to another instance).
    //! \post The copied-from \a src cannot be destructed in the normal manner.
    void copy_from_unsafe(FunctionData const& src) noexcept
    {
        OMNI_FUNCTION_PUSH_IGNORE_CLASS_MEMACCESS()
        std::memcpy(this, &src, sizeof(*this));
        OMNI_FUNCTION_POP_IGNORE_CLASS_MEMACCESS()
    }

    //! Assign the function or functor \a f with the \c Assigner.
    //!
    //! \tparam Assigner The appropriate \c FunctionAssigner type for the \c FunctionTraits and \c F we are trying to
    //!         bind.
    //! \pre This function must be either uninitialized or otherwise clear. This function will overwrite any state
    //!      without checking if it is engaged ahead of time. As such, it should only be called from a constructor or
    //!      after a \c reset.
    template <typename Assigner, typename F>
    CARB_NODISCARD Result bind_unsafe(Assigner, F&& f)
    {
        if (!Assigner::is_active(f))
        {
            clear_unsafe();
            return core::kResultSuccess;
        }

        if (Result rc = Assigner::initialize(m_buffer, std::forward<F>(f)))
            return rc;

        m_trampoline = reinterpret_cast<void*>(&Assigner::trampoline);
        m_characteristics = Assigner::characteristics();
        return core::kResultSuccess;
    }

    void reset() noexcept
    {
        if (m_characteristics && m_characteristics->destroy)
            m_characteristics->destroy(&m_buffer);

        clear_unsafe();
    }

    void swap(FunctionData& other) noexcept
    {
        if (this == &other)
            return;

        std::array<char, sizeof(*this)> temp;
        std::memcpy(temp.data(), this, sizeof(temp));
        std::memcpy(this, &other, sizeof(*this));
        OMNI_FUNCTION_PUSH_IGNORE_CLASS_MEMACCESS()
        std::memcpy(&other, temp.data(), sizeof(other));
        OMNI_FUNCTION_POP_IGNORE_CLASS_MEMACCESS()
    }
};

static_assert(sizeof(FunctionData) == kFUNCTION_SIZE, "FunctionData incorrect size");
static_assert(alignof(FunctionData) == kFUNCTION_BUFFER_ALIGN, "FunctionData incorrect alignment");

struct TrivialFunctorCharacteristics
{
    static FunctionCharacteristics const* characteristics() noexcept
    {
        return nullptr;
    }
};

template <typename Signature>
struct FunctionTraits;

template <typename TReturn, typename... TArgs>
struct FunctionTraits<TReturn(TArgs...)>
{
    using result_type = TReturn;
};

//! The "assigner" for a function is used in bind operations. It is responsible for managing copy and delete operations,
//! detecting if a binding target is non-null, and creating a trampoline function to bridge the gap between the function
//! signature `TReturn(TArgs...)` and the target function signature.
//!
//! When the bind \c Target is not bindable-to, this base case is matched, which has no member typedef \c type. This is
//! used in \c omni::function to disqualify types which cannot be bound to. When the \c Target is bindable (see the
//! partial specializations below), the member \c type is a structure with the following static functions:
//!
//! * `TReturn trampoline(FunctionBuffer const*, TArgs...)` -- The trampoline function performs an arbitrary action to
//!   convert `TArgs...` and `TReturn` of the actual call.
//! * `FunctionCharacteristics const* characteristics() noexcept` -- Give the singleton which describes the managment
//!   operations for this target.
//! * `bool is_active(Target const&) noexcept` -- Detects if the \c Target is null or not.
//! * `Result initialize(FunctionBuffer&, Target&&)` -- Initializes the buffer from the provided target.
//!
//! See \c FunctionAssigner
template <typename TTraits, typename Target, typename = void>
struct FunctionAssignerImpl
{
};

//! Binding for any pointer-like type.
template <typename TReturn, typename... TArgs, typename TPointer>
struct FunctionAssignerImpl<FunctionTraits<TReturn(TArgs...)>,
                            TPointer,
                            std::enable_if_t<conjunction<is_invocable_r<TReturn, TPointer, TArgs...>,
                                                         disjunction<std::is_pointer<TPointer>,
                                                                     std::is_member_function_pointer<TPointer>,
                                                                     std::is_member_object_pointer<TPointer>>>::value>>
{
    struct type : TrivialFunctorCharacteristics
    {
        static TReturn trampoline(FunctionBuffer const* buf, TArgs... args)
        {
            auto realfn = const_cast<TPointer*>(reinterpret_cast<TPointer const*>(buf->raw.data()));
            // NOTE: The use of invoke_r here is for cases where TReturn is void, but `(*realfn)(args...)` is not. It is
            // fully qualified to prevent ADL ambiguity with std::invoke_r when any of TArgs reside in the std
            // namespace.
            return ::carb::cpp17::invoke_r<TReturn>(*realfn, std::forward<TArgs>(args)...);
        }

        CARB_NODISCARD static Result initialize(FunctionBuffer& buffer, TPointer fn) noexcept
        {
            static_assert(sizeof(fn) <= sizeof(buffer.raw), "Function too large to fit in buffer");
            std::memcpy(buffer.raw.data(), &fn, sizeof(fn));
            return 0;
        }

        static bool is_active(TPointer func) noexcept
        {
            return bool(func);
        }
    };
};

//! Binding to a class or union type with `operator()`.
template <typename TReturn, typename... TArgs, typename Functor>
struct FunctionAssignerImpl<FunctionTraits<TReturn(TArgs...)>,
                            Functor,
                            std::enable_if_t<conjunction<
                                // do not match exactly this function
                                negation<std::is_same<Functor, omni::function<TReturn(TArgs...)>>>,
                                // must be a class or union type (not a pointer or reference to a function)
                                disjunction<std::is_class<Functor>, std::is_union<Functor>>,
                                // must be callable
                                is_invocable_r<TReturn, Functor&, TArgs...>>::value>>
{
    struct InBuffer : TrivialFunctorCharacteristics
    {
        static TReturn trampoline(FunctionBuffer const* buf, TArgs... args)
        {
            auto real = const_cast<Functor*>(reinterpret_cast<Functor const*>(buf->raw.data()));
            // NOTE: invoke_r is fully qualified to prevent ADL ambiguity with std::invoke_r when any of TArgs reside in
            // the std namespace.
            return ::carb::cpp17::invoke_r<TReturn>(*real, std::forward<TArgs>(args)...);
        }

        template <typename F>
        CARB_NODISCARD static Result initialize(FunctionBuffer& buffer, F&& func)
        {
            static_assert(sizeof(func) <= sizeof(buffer), "Provided functor is too large to fit in buffer");

// GCC 7.1-7.3 has a compiler bug where a diagnostic for uninitialized `func` is emitted in cases where it refers to a
// lambda expression which has been `forward`ed multiple times. This is erroneous in GCC 7.1-7.3, but other compilers
// complaining about uninitialized access might have a valid point.
#if CARB_COMPILER_GNUC && !CARB_TOOLCHAIN_CLANG && __GNUC__ == 7 && __GNUC_MINOR__ <= 3
            CARB_IGNOREWARNING_GNUC_WITH_PUSH("-Wmaybe-uninitialized")
#endif
            std::memcpy(buffer.raw.data(), &func, sizeof(func));
#if CARB_COMPILER_GNUC && !CARB_TOOLCHAIN_CLANG && __GNUC__ == 7 && __GNUC_MINOR__ <= 3
            CARB_IGNOREWARNING_GNUC_POP
#endif
            return 0;
        }

        static constexpr bool is_active(Functor const&) noexcept
        {
            return true;
        }
    };

    struct InBufferWithDestructor : InBuffer
    {
        static FunctionCharacteristics const* characteristics() noexcept
        {
            static FunctionCharacteristics const instance{
                [](FunctionBuffer* self) { reinterpret_cast<Functor*>(self->raw.data())->~Functor(); },
                nullptr,
            };
            return &instance;
        }
    };

    struct HeapAllocated
    {
        template <typename... AllocArgs>
        static Functor* make(AllocArgs&&... args)
        {
            std::unique_ptr<void, void (*)(void*)> tmp{ carb::allocate(sizeof(Functor), alignof(Functor)),
                                                        &carb::deallocate };
            if (!tmp)
                return nullptr;
            new (tmp.get()) Functor{ std::forward<AllocArgs>(args)... };
            return static_cast<Functor*>(tmp.release());
        }

        CARB_NODISCARD static Result clone(FunctionBuffer* target, FunctionBuffer const* source)
        {
            if (auto created = make(*static_cast<Functor const*>(source->pointer)))
            {
                target->pointer = static_cast<void*>(created);
                return core::kResultSuccess;
            }
            else
            {
                return core::kResultFail;
            }
        }

        static void destroy(FunctionBuffer* buffer) noexcept
        {
            auto real = static_cast<Functor*>(buffer->pointer);
            real->~Functor();
            carb::deallocate(real);
        }

        static TReturn trampoline(FunctionBuffer const* buf, TArgs... args)
        {
            auto real = static_cast<Functor*>(buf->pointer);
            return ::carb::cpp17::invoke_r<TReturn>(*real, std::forward<TArgs>(args)...);
        }

        template <typename Signature>
        static bool is_active(omni::function<Signature> const& f) noexcept
        {
            // omni::function derives from FunctionData, so this is a correct cast, even though it is not defined yet.
            // The use of reinterpret_cast is required here because omni::function derives privately.
            auto underlying = reinterpret_cast<FunctionData const*>(&f);
            return underlying->m_trampoline;
        }

        template <typename Signature>
        static bool is_active(std::function<Signature> const& f) noexcept
        {
            return bool(f);
        }

        template <typename UFunctor>
        static bool is_active(UFunctor const&) noexcept
        {
            return true;
        }

        template <typename UFunctor>
        CARB_NODISCARD static Result initialize(FunctionBuffer& buffer, UFunctor&& func)
        {
            if (auto created = make(std::forward<UFunctor>(func)))
            {
                buffer.pointer = static_cast<void*>(created);
                return core::kResultSuccess;
            }
            else
            {
                // we can't guarantee that anything in make sets the error code, so use the generic "fail"
                return core::kResultFail;
            }
        }

        static FunctionCharacteristics const* characteristics() noexcept
        {
            static FunctionCharacteristics const instance{
                &destroy,
                &clone,
            };
            return &instance;
        }
    };

    // clang-format off
    using type =
        std::conditional_t<
            conjunction<bool_constant<(sizeof(Functor) <= kFUNCTION_BUFFER_SIZE && kFUNCTION_BUFFER_ALIGN % alignof(Functor) == 0)>,
                        // we're really looking for relocatability, but trivial copy will do for now
                        std::is_trivially_copyable<Functor>
                       >::value,
            std::conditional_t<std::is_trivially_destructible<Functor>::value,
                               InBuffer,
                               InBufferWithDestructor>,
            HeapAllocated>;
    // clang-format on
};

//! Choose the proper function assigner from its properties. This will fail in a SFINAE-safe manner if the \c Target is
//! not bindable to a function with the given \c TTraits.
template <typename TTraits, typename Target>
using FunctionAssigner = typename FunctionAssignerImpl<TTraits, std::decay_t<Target>>::type;

CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE()

} // namespace detail
//! \endcond
} // namespace omni
