// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Carbonite Delegate implementation.
#pragma once

#include "../Defines.h"

#include "../Strong.h"
#include "../container/IntrusiveList.h"
#include "../cpp17/Tuple.h"
#include "../thread/Mutex.h"
#include "../thread/Util.h"

#include <type_traits>
#include <vector>
#include <memory>

namespace carb
{

//! Namespace for Carbonite delegate implementation.
namespace delegate
{


template <class T>
class Delegate;

template <class T>
class DelegateRef;

/**
 * Implements a thread-safe callback system that can have multiple subscribers.
 *
 * A delegate is a weak-coupling callback system. Essentially, a system uses Delegate to have a callback that can
 * be received by multiple subscribers.
 *
 * Delegate has two ways to uniquely identify a bound callback: \ref Bind() will output a \ref Handle, or the
 * caller can provide a key of any type with BindWithKey(). Either the \ref Handle or the given key can be passed to
 * \ref Unbind() in order to remove a callback.
 *
 * Delegate can call all bound callbacks with the Call() function. Recursive calling is allowed with caveats listed
 * below.
 *
 * Delegate is thread-safe for all operations. Call() can occur simultaneously in multiple threads. An Unbind()
 * will wait if the bound callback is currently executing in another thread.
 *
 * Delegate can be destroyed from a binding (during \ref Call()) as the internal state is not disposed of
 * until all active calls have been completed. See ~Delegate().
 *
 * Delegate does not hold any internal locks while calling bound callbacks. It is strongly recommended to avoid
 * holding locks when invoking Delegate's \ref Call() function.
 *
 * These tenets make up the basis of Carbonite's Basic Callback Hygiene as described in the @rstdoc{../../../../CODING}.
 */
template <class... Args>
class Delegate<void(Args...)>
{
public:
    //! A type representing the function type
    using FunctionType = void(Args...);

    /**
     * A quasi-unique identifier outputted from Bind()
     *
     * \ref Handle is unique as long as it has not rolled over.
     */
    CARB_STRONGTYPE(Handle, size_t);
    CARB_DOC_CONSTEXPR static Handle kInvalidHandle{ 0 }; //!< A value representing an invalid \ref Handle value

    /**
     * Constructs an empty delegate
     */
    Delegate() = default;

    /**
     * Move constructor.
     *
     * @param other The Delegate to move from. This Delegate will be left in a valid but empty state.
     */
    Delegate(Delegate&& other);

    /**
     * Move-assign operator
     *
     * @param other The Delegate to move-assign from. Will be swapped with `*this`.
     *
     * @returns `*this`
     */
    Delegate& operator=(Delegate&& other);

    /**
     * Destructor.
     *
     * The destructor unbinds all bindings and follows the waiting paradigm explained by \ref UnbindAll(). As the
     * internal state of the delegate is held until all active calls have completed, it is valid to destroy Delegate
     * from a callback.
     */
    ~Delegate();

    /**
     * Binds a callable (with optional additional arguments) to the delegate.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * \note This function can be done from within a callback. If done during a callback, the newly bound callable will
     * not be available to be called until \ref Call() returns, at which point the callback can be called by other
     * threads or outer \ref Call() calls (in the case of recursive calls to \ref Call()).
     *
     * @param hOut An optional pointer that receives a \ref Handle representing the binding to \c Callable. This can
     * be \c nullptr to ignore the \ref Handle. The same \ref Handle is also returned. In a multi-threaded environment,
     * it is possible for \p func to be called before \ref Bind() returns, but \p hOut will have already been assigned.
     * @param func A callable object, such as lambda, functor or [member-]function. Return values are ignored. The
     * callable must take as parameters \p args followed by the \c Args declared in the delegate template signature.
     * @param args Additional optional arguments to bind with \p func. If \p func is a member function pointer the
     * first argument must be the \c this pointer to call the member function with.
     * @return The \ref Handle also passed to \p hOut.
     */
    template <class Callable, class... BindArgs>
    Handle Bind(Handle* hOut, Callable&& func, BindArgs&&... args);

    /**
     * Binds a callable (with optional additional arguments) to the delegate with a user-defined key.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * \note This function can be done from within a callback. If done during a callback, the newly bound callable will
     * not be available to be called until \ref Call() returns, at which point the callback can be called by other
     * threads or outer \ref Call() calls (in the case of recursive calls to \ref Call()).
     *
     * @param key A user-defined key of any type that supports equality (==) to identify this binding. Although multiple
     * bindings can be referenced by the same key, Unbind() will only remove a single binding.
     * @param func A callable object, such as lambda, functor or [member-]function. Return values are ignored. The
     * callable must take as parameters \p args followed by the \c Args declared in the delegate template signature.
     * @param args Additional optional arguments to bind with \p func. If \p func is a member function pointer the
     * first argument must be the \c this pointer to call the member function with.
     */
    template <class KeyType, class Callable, class... BindArgs>
    void BindWithKey(KeyType&& key, Callable&& func, BindArgs&&... args);

    /**
     * Unbinds any single binding referenced by the given key.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * This function can be done from within a callback. If the referenced binding is currently executing in
     * another thread, Unbind() will not return until it has finished. Any binding can be safely unbound during a
     * callback. If a binding un-binds itself, the captured arguments and callable object will not be destroyed
     * until just before \ref Call() returns.
     *
     * \note It is guaranteed that when \ref Unbind() returns, the callback is not running and will never run in any
     * threads.
     *
     * @param key A \ref Handle or user-defined key previously passed to \ref BindWithKey().
     * @return \c true if a binding was un-bound; \c false if no binding matching key was found.
     */
    template <class KeyType>
    bool Unbind(KeyType&& key);

    /**
     * Indicates if a binding exists in `*this` with the given key or Handle.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     * However, without external synchronization, it is possible for the result of this function to be incorrect by the
     * time it is used.
     *
     * @param key A \ref Handle or user-defined key previously passed to \ref BindWithKey().
     * @returns \c true if a binding exists with the given \p key; \c false if no binding matching key was found.
     */
    template <class KeyType>
    bool HasKey(KeyType&& key) const noexcept;

    /**
     * Unbinds the currently executing callback without needing an identifying key.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * \note If not done within the context of a callback, this function has no effect.
     *
     * @return \c true if a binding was un-bound; \c false if there is no current binding.
     */
    bool UnbindCurrent();

    /**
     * Unbinds all bound callbacks, possibly waiting for active calls to complete.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * Unbinds all currently bound callbacks. This function will wait to return until bindings that it unbinds have
     * completed all calls in other threads. It is safe to perform this operation from within a callback.
     */
    void UnbindAll();

    /**
     * Returns the number of active bound callbacks.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * \note This function returns the count of \a active bound callbacks only. Pending callbacks (that were added with
     * \ref Bind() during \ref Call()) are not counted. Use \ref HasPending() to determine if pending bindings exist.
     *
     * @returns the number of active bound callbacks.
     */
    size_t Count() const noexcept;

    /**
     * Checks whether the Delegate has any pending bindings.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     * The nature of this function is such that the result may be stale by the time it is read in the calling thread,
     * unless the calling thread has at least one pending binding.
     *
     * \note This function returns \c true if any \a pending bound callbacks exist. This will only ever be non-zero if
     * one or more threads are currently in the \ref Call() function.
     *
     * @returns \c true if any pending bindings exist; \c false otherwise.
     */
    bool HasPending() const noexcept;

    /**
     * Checks whether the Delegate contains no pending or active bound callbacks.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     * However, without external synchronization, it is possible for the result of this function to be incorrect by the
     * time it is used.
     *
     * @returns \c true if there are no active or pending callbacks present in `*this`; \c false otherwise.
     */
    bool IsEmpty() const noexcept;

    /**
     * Given a type, returns a \c std::vector containing a copy of all keys used for bindings.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * \note This function can be done from within a callback. Pending callbacks (that were added with \ref Bind()
     * during \ref Call()) are included, even if they are pending in other threads. Note that in a multi-threaded
     * environment, the actual keys in use by Delegate may change after this function returns; in such cases, an
     * external mutex is recommended. \c KeyType must be Copyable in order for this function to compile.
     *
     * @tparam KeyType \ref Handle or a type previously passed to \ref BindWithKey()
     * @return a \c std::vector of copies of keys of the given type in use by this Delegate.
     */
    template <class KeyType>
    std::vector<std::decay_t<KeyType>> GetKeysByType() const;

    /**
     * Calls all bound callbacks for this Delegate.
     *
     * \thread_safety: Thread-safe with respect to other Delegate operations except for construction and destruction.
     *
     * \note This function can be done concurrently in multiple threads simultaneously. Recursive calls to \ref Call()
     * are allowed but the caller must take care to avoid endless recursion. Callbacks are free to call \ref Bind(),
     * \ref Unbind() or any other Delegate function. No internal locks are held while callbacks are called.
     *
     * @param args The arguments to pass to the callbacks.
     */
    void Call(Args... args);

    /**
     * Syntactic sugar for \ref Call()
     */
    void operator()(Args... args);

    /**
     * Swaps with another Delegate.
     *
     * @param other The Delegate to swap with.
     */
    void swap(Delegate& other);

    CARB_PREVENT_COPY(Delegate);

private:
    template <class U>
    friend class DelegateRef;

    struct BaseBinding;
    template <class Key>
    struct KeyedBinding;
    using Container = carb::container::IntrusiveList<BaseBinding, &BaseBinding::link>;

    struct ActiveCall;
    using ActiveCallList = carb::container::IntrusiveList<ActiveCall, &ActiveCall::link>;

    struct Impl : public std::enable_shared_from_this<Impl>
    {
        mutable carb::thread::mutex m_mutex;
        Container m_entries;
        ActiveCallList m_activeCalls;

        ~Impl();
    };

    constexpr Delegate(std::nullptr_t);
    Delegate(std::shared_ptr<Impl> pImpl);

    ActiveCall* lastCurrentThreadCall();
    const ActiveCall* lastCurrentThreadCall() const;
    void UnbindInternal(std::unique_lock<carb::thread::mutex>& g, typename Container::iterator iter);

    static size_t nextHandle();

    std::shared_ptr<Impl> m_impl{ std::make_shared<Impl>() };
};

/**
 * Holds a reference to a Delegate.
 *
 * Though Delegate is non-copyable, \c DelegateRef can be thought of as a `std::shared_ptr` for Delegate.
 * This allows a Delegate's bindings to remain active even though the original Delegate has been destroyed, which can
 * allow calls in progress to complete, or a mutex protecting the original Delegate to be unlocked.
 */
template <class... Args>
class DelegateRef<void(Args...)>
{
public:
    //! The Delegate type that is referenced.
    using DelegateType = Delegate<void(Args...)>;

    /**
     * Default constructor.
     *
     * Creates an empty DelegateRef such that `bool(*this)` would be `false`.
     */
    constexpr DelegateRef() noexcept;

    /**
     * Constructor.
     *
     * Constructs a DelegateRef that holds a strong reference to \p delegate.
     * @param delegate The Delegate object to hold a reference to.
     */
    explicit DelegateRef(DelegateType& delegate);

    /**
     * Copy constructor.
     *
     * References the same underlying Delegate that \p other references. If \p other is empty, `*this` will also be
     * empty.
     * @param other A DelegateRef to copy.
     */
    DelegateRef(const DelegateRef& other);

    /**
     * Move constructor.
     *
     * Moves the reference from \p other to `*this`. If \p other is empty, `*this` will also be empty. \p other is left
     * in a valid but empty state.
     * @param other A DelegateRef to move.
     */
    DelegateRef(DelegateRef&& other) = default;

    /**
     * Destructor
     */
    ~DelegateRef();

    /**
     * Copy-assign.
     *
     * References the same underlying Delegate that \p other references and releases any existing reference. The order
     * of these operations is unspecified, so assignment from `*this` is undefined behavior.
     * @param other A DelegateRef to copy.
     * @returns `*this`.
     */
    DelegateRef& operator=(const DelegateRef& other);

    /**
     * Move-assign.
     *
     * Moves the reference from \p other to `*this` and releases any existing reference. The order of these operations
     * is unspecified, so assignment from `*this` is undefined behavior. If \p other is empty, `*this` will also be
     * empty. \p other is left in a valid but empty state.
     * @param other A DelegateRef to move.
     * @returns `*this`.
     */
    DelegateRef& operator=(DelegateRef&& other) = default;

    /**
     * Checks whether the DelegateRef holds a valid reference.
     * @returns `true` if `*this` holds a valid reference; `false` otherwise.
     */
    explicit operator bool() const noexcept;

    /**
     * Clears the DelegateRef to an empty reference.
     *
     * Postcondition: `bool(*this)` will be `false`.
     */
    void reset();

    /**
     * References a different Delegate and releases any existing reference.
     * @param delegate The Delegate to reference.
     */
    void reset(DelegateType& delegate);

    /**
     * Swaps the reference with another DelegateRef.
     * @param other A DelegateRef to swap with.
     */
    void swap(DelegateRef& other);

    /**
     * Retrieves the underlying DelegateType.
     * @returns a pointer to the referenced Delegate, or `nullptr` if `bool(*this)` would return false.
     */
    DelegateType* get() const noexcept;

    /**
     * Dereferences *this.
     * @returns a reference to the referenced Delegate. If `bool(*this)` would return false, behavior is undefined.
     */
    DelegateType& operator*() const noexcept;

    /**
     * Dereferences *this.
     * @returns a pointer to the referenced Delegate. If `bool(*this)` would return false, behavior is undefined.
     */
    DelegateType* operator->() const noexcept;

private:
    DelegateType m_delegate;
};

//! A helper class for determining the type of a \ref carb::delegate::DelegateRef based on a
//! \ref carb::delegate::Delegate.  @tparam Del a \ref carb::delegate::Delegate
template <class Del>
struct RefFromDelegate
{
    //! The type of \ref DelegateRef that should be used for a \ref Delegate of type `Del`
    using type = DelegateRef<typename Del::FunctionType>;
};

//! Definition helper for `RefFromDelegate<Del>::type`
template <class Del>
using RefFromDelegate_t = typename RefFromDelegate<Del>::type;

} // namespace delegate

} // namespace carb

#include "DelegateImpl.inl"

CARB_INCLUDE_PURIFY_TEST({
    using namespace carb::delegate;
    Delegate<void()> d, d2{ Delegate<void()>{} }, d3 = Delegate<void()>();
    auto b = d.Bind(nullptr, [] {});
    d.Bind(nullptr, [](bool) {}, true);
    d.BindWithKey(0, [] {});
    d.BindWithKey(1, [](bool) {}, false);
    d.Unbind(1);
    d.Unbind(b);
    d.HasKey(0);
    d.UnbindCurrent();
    d2.UnbindAll();
    d.Count();
    d.HasPending();
    d.IsEmpty();
    d.GetKeysByType<int>();
    d.Call();
    d();
    d.swap(d3);

    DelegateRef<void()> dr(d), dr2{};
    DelegateRef<void()> dr3(dr);
    DelegateRef<void()> dr4(std::move(dr2));
    DelegateRef<void()> dr5 = std::move(dr4);
    DelegateRef<void()> dr6 = dr5;
    CARB_UNUSED(bool(dr6));
    dr6.reset();
    dr5.reset(d);
    dr5.get();
    (*dr).Call();
    dr->Call();
});
