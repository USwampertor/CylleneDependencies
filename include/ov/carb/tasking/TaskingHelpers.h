// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief carb.tasking helper functions
#pragma once

#include "TaskingTypes.h"

#include "../thread/Futex.h"
#include "../cpp17/Functional.h"
#include "../cpp17/Optional.h"
#include "../cpp17/Variant.h"

#include <atomic>
#include <chrono>
#include <iterator>
#include <vector>

namespace carb
{
namespace tasking
{

#ifndef DOXYGEN_BUILD
namespace detail
{

template <class T>
struct is_literal_string
{
    constexpr static bool value = false;
};

template <size_t N>
struct is_literal_string<const char (&)[N]>
{
    constexpr static bool value = true;
};

Counter* const kListOfCounters{ (Counter*)(size_t)-1 };

template <class Rep, class Period>
uint64_t convertDuration(const std::chrono::duration<Rep, Period>& dur)
{
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(thread::detail::clampDuration(dur)).count();
    return uint64_t(::carb_max(std::chrono::nanoseconds::rep(0), ns));
}

template <class Clock, class Duration>
uint64_t convertAbsTime(const std::chrono::time_point<Clock, Duration>& tp)
{
    return convertDuration(tp - Clock::now());
}

template <class F, class Tuple, size_t... I, class... Args>
decltype(auto) applyExtraImpl(F&& f, Tuple&& t, std::index_sequence<I...>, Args&&... args)
{
    CARB_UNUSED(t); // Can get C4100: unreferenced formal parameter on MSVC when Tuple is empty.
    return cpp17::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))..., std::forward<Args>(args)...);
}

template <class F, class Tuple, class... Args>
decltype(auto) applyExtra(F&& f, Tuple&& t, Args&&... args)
{
    return applyExtraImpl(std::forward<F>(f), std::forward<Tuple>(t),
                          std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{},
                          std::forward<Args>(args)...);
}

// U looks like an iterator convertible to V when dereferenced
template <class U, class V>
using IsForwardIter = carb::cpp17::conjunction<
    carb::cpp17::negation<
        typename std::is_convertible<typename std::iterator_traits<U>::iterator_category, std::random_access_iterator_tag>>,
    typename std::is_convertible<typename std::iterator_traits<U>::iterator_category, std::forward_iterator_tag>,
    std::is_convertible<decltype(*std::declval<U&>()), V>>;

template <class U, class V>
using IsRandomAccessIter = carb::cpp17::conjunction<
    typename std::is_convertible<typename std::iterator_traits<U>::iterator_category, std::random_access_iterator_tag>,
    std::is_convertible<decltype(*std::declval<U&>()), V>>;

// Must fit within a pointer, be trivially move constructible and trivially destructible.
template <class Functor>
using FitsWithinPointerTrivially =
    carb::cpp17::conjunction<carb::cpp17::bool_constant<sizeof(typename std::decay_t<Functor>) <= sizeof(void*)>,
                             std::is_trivially_move_constructible<typename std::decay_t<Functor>>,
                             std::is_trivially_destructible<typename std::decay_t<Functor>>>;

template <class Functor, std::enable_if_t<FitsWithinPointerTrivially<Functor>::value, bool> = false>
inline void generateTaskFunc(TaskDesc& desc, Functor&& func)
{
    // Use SFINAE to have this version of generateTaskFunc() contribute to resolution only if Functor will fit within a
    // void*, so that we can use the taskArg as the instance. On my machine, this is about a tenth of the time for the
    // below specialization, and happens more frequently.
    using Func = typename std::decay_t<Functor>;
    union
    {
        Func f;
        void* v;
    } u{ std::forward<Functor>(func) };
    desc.taskArg = u.v;
    desc.task = [](void* arg) {
        union CARB_ATTRIBUTE(visibility("hidden"))
        {
            void* v;
            Func f;
        } u{ arg };
        u.f();
    };
    // Func is trivially destructible so we don't need a cancel func
}

template <class Functor, std::enable_if_t<!FitsWithinPointerTrivially<Functor>::value, bool> = false>
inline void generateTaskFunc(TaskDesc& desc, Functor&& func)
{
    // Use SFINAE to have this version of generateTaskFunc() contribute to resolution only if Functor will NOT fit
    // within a void*, so that the heap can be used only if necessary
    using Func = typename std::decay_t<Functor>;
    // Need to allocate
    desc.taskArg = new Func(std::forward<Functor>(func));
    desc.task = [](void* arg) {
        std::unique_ptr<Func> p(static_cast<Func*>(arg));
        (*p)();
    };
    desc.cancel = [](void* arg) { delete reinterpret_cast<Func*>(arg); };
}

template <class T>
class SharedState;

template <>
class SharedState<void>
{
    std::atomic_size_t m_refs;

public:
    SharedState(bool futureRetrieved) noexcept : m_refs(1 + futureRetrieved), m_futureRetrieved(futureRetrieved)
    {
    }
    virtual ~SharedState() = default;

    void addRef() noexcept
    {
        m_refs.fetch_add(1, std::memory_order_relaxed);
    }

    void release()
    {
        if (m_refs.fetch_sub(1, std::memory_order_release) == 1)
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
    }

    void set()
    {
        CARB_FATAL_UNLESS(m_futex.exchange(isTask() ? eTaskPending : eReady, std::memory_order_acq_rel) == eUnset,
                          "Value already set");
    }
    void get()
    {
    }

    void notify();

    void markReady()
    {
        m_futex.store(eReady, std::memory_order_release);
    }

    bool ready() const
    {
        return m_futex.load(std::memory_order_relaxed) == eReady;
    }

    bool isTask() const
    {
        return m_object.type == ObjectType::eTaskContext;
    }

    enum State : uint8_t
    {
        eReady = 0,
        eUnset,
        eInProgress,
        eTaskPending,
    };

    std::atomic<State> m_futex{ eUnset };
    std::atomic_bool m_futureRetrieved{ false };
    Object m_object{ ObjectType::eFutex1, &m_futex };
};

template <class T>
class SharedState<T&> final : public SharedState<void>
{
public:
    SharedState(bool futureRetrieved) noexcept : SharedState<void>(futureRetrieved)
    {
    }

    bool isSet() const noexcept
    {
        return m_value != nullptr;
    }

    T& get() const
    {
        CARB_FATAL_UNLESS(m_value, "Attempting to retrieve value from broken promise");
        return *m_value;
    }
    void set(T& val)
    {
        CARB_FATAL_UNLESS(m_futex.exchange(eInProgress, std::memory_order_acquire) == 1, "Value already set");
        m_value = std::addressof(val);
        m_futex.store(this->isTask() ? eTaskPending : eReady, std::memory_order_release);
    }

    T* m_value{ nullptr };
};

template <class T>
class SharedState final : public SharedState<void>
{
public:
    using Type = typename std::decay<T>::type;

    SharedState(bool futureRetrieved) noexcept : SharedState<void>(futureRetrieved)
    {
    }

    bool isSet() const noexcept
    {
        return m_type.has_value();
    }

    const T& get_ref() const
    {
        CARB_FATAL_UNLESS(m_type, "Attempting to retrieve value from broken promise");
        return m_type.value();
    }
    T get()
    {
        CARB_FATAL_UNLESS(m_type, "Attempting to retrieve value from broken promise");
        return std::move(m_type.value());
    }
    void set(const T& value)
    {
        CARB_FATAL_UNLESS(m_futex.exchange(eInProgress, std::memory_order_acquire) == 1, "Value already set");
        m_type.emplace(value);
        m_futex.store(this->isTask() ? eTaskPending : eReady, std::memory_order_release);
    }
    void set(T&& value)
    {
        CARB_FATAL_UNLESS(m_futex.exchange(eInProgress, std::memory_order_acquire) == 1, "Value already set");
        m_type.emplace(std::move(value));
        m_futex.store(this->isTask() ? eTaskPending : eReady, std::memory_order_release);
    }

    carb::cpp17::optional<Type> m_type;
};

} // namespace detail
#endif

class TaskGroup;

/**
 * Helper class to ensure correct compliance with the requiredObject parameter of ITasking::add[Throttled]SubTask() and
 * wait() functions.
 *
 * The following may be converted into a RequiredObject: TaskContext, Future, Any, All, Counter*, or CounterWrapper.
 */
struct RequiredObject final : public Object
{
    /**
     * Constructor that accepts a `std::nullptr_t`.
     */
    constexpr RequiredObject(std::nullptr_t) : Object{ ObjectType::eNone, nullptr }
    {
    }

    /**
     * Constructor that accepts an object that can be converted to Counter*.
     *
     * @param c An object convertible to Counter*. This can be Any, All, Counter* or CounterWrapper.
     */
    template <class T, std::enable_if_t<std::is_convertible<T, Counter*>::value, bool> = false>
    constexpr RequiredObject(T&& c) : Object{ ObjectType::eCounter, static_cast<Counter*>(c) }
    {
    }

    /**
     * Constructor that accepts an object that can be converted to TaskContext.
     *
     * @param tc A TaskContext or object convertible to TaskContext, such as a Future.
     */
    template <class T, std::enable_if_t<std::is_convertible<T, TaskContext>::value, bool> = true>
    constexpr RequiredObject(T&& tc)
        : Object{ ObjectType::eTaskContext, reinterpret_cast<void*>(static_cast<TaskContext>(tc)) }
    {
    }

    /**
     * Constructor that accepts a TaskGroup&.
     */
    constexpr RequiredObject(const TaskGroup& tg);

    /**
     * Constructor that accepts a TaskGroup*. `nullptr` may be provided.
     */
    constexpr RequiredObject(const TaskGroup* tg);

private:
    friend struct ITasking;
    template <class U>
    friend class Future;
    template <class U>
    friend class SharedFuture;

    constexpr RequiredObject(const Object& o) : Object(o)
    {
    }

    void get(TaskDesc& desc);
};

/**
 * Specifies an "all" grouping of RequiredObject(s).
 *
 * @note *ALL* RequiredObject(s) given in the constructor must become signaled before the All object will be considered
 * signaled.
 *
 * All and Any objects can be nested as they are convertible to RequiredObject.
 */
struct All final
{
    /**
     * Constructor that accepts an initializer_list of RequiredObject(s).
     * @param il The `initializer_list` of RequiredObject(s).
     */
    All(std::initializer_list<RequiredObject> il);

    /**
     * Constructor that accepts begin and end iterators that produce RequiredObject objects.
     * @param begin The beginning iterator.
     * @param end An off-the-end iterator just beyond the end of the list.
     */
    template <class InputIt, std::enable_if_t<detail::IsForwardIter<InputIt, RequiredObject>::value, bool> = false>
    All(InputIt begin, InputIt end);

    //! @private
    template <class InputIt, std::enable_if_t<detail::IsRandomAccessIter<InputIt, RequiredObject>::value, bool> = false>
    All(InputIt begin, InputIt end);

    /**
     * Convertible to RequiredObject.
     */
    operator RequiredObject() const
    {
        return RequiredObject(m_counter);
    }

private:
    friend struct RequiredObject;
    Counter* m_counter;

    operator Counter*() const
    {
        return m_counter;
    }
};

/**
 * Specifies an "any" grouping of RequiredObject(s).
 *
 * @note *ANY* RequiredObject given in the constructor that is or becomes signaled will cause the Any object to become
 * signaled.
 *
 * All and Any objects can be nested as they are convertible to RequiredObject.
 */
struct Any final
{
    /**
     * Constructor that accepts an initializer_list of RequiredObject objects.
     * @param il The initializer_list of RequiredObject objects.
     */
    Any(std::initializer_list<RequiredObject> il);

    /**
     * Constructor that accepts begin and end iterators that produce RequiredObject objects.
     * @param begin The beginning iterator.
     * @param end An off-the-end iterator just beyond the end of the list.
     */
    template <class InputIt, std::enable_if_t<detail::IsForwardIter<InputIt, RequiredObject>::value, bool> = false>
    Any(InputIt begin, InputIt end);

    //! @private
    template <class InputIt, std::enable_if_t<detail::IsRandomAccessIter<InputIt, RequiredObject>::value, bool> = false>
    Any(InputIt begin, InputIt end);

    /**
     * Convertible to RequiredObject.
     */
    operator RequiredObject() const
    {
        return RequiredObject(m_counter);
    }

private:
    friend struct RequiredObject;
    Counter* m_counter;

    operator Counter*() const
    {
        return m_counter;
    }
};

/**
 * Helper class to provide correct types to the Trackers class.
 *
 * The following types are valid trackers:
 * - Anything convertible to Counter*, such as CounterWrapper. Counters are deprecated however. The Counter is
 *   incremented before the task can possibly begin executing and decremented when the task finishes.
 * - Future<void>&: This can be used to atomically populate a Future<void> before the task could possibly start
 *   executing.
 * - Future<void>*: Can be `nullptr`, but if not, can be used to atomically populate a Future<void> before the task
 *   could possibly start executing.
 * - TaskContext&: By providing a reference to a TaskContext it will be atomically filled before the task could possibly
 *   begin executing.
 * - TaskContext*: By providing a pointer to a TaskContext (that can be `nullptr`), it will be atomically filled before
 *   the task could possibly begin executing, if valid.
 */
struct Tracker final : Object
{
    /**
     * Constructor that accepts a `std::nullptr_t`.
     */
    constexpr Tracker(std::nullptr_t) : Object{ ObjectType::eNone, nullptr }
    {
    }

    /**
     * Constructor that accepts a Counter* or an object convertible to Counter*, such as CounterWrapper.
     *
     * @param c The object convertible to Counter*.
     */
    template <class T, std::enable_if_t<std::is_convertible<T, Counter*>::value, bool> = false>
    constexpr Tracker(T&& c) : Object{ ObjectType::eCounter, reinterpret_cast<void*>(static_cast<Counter*>(c)) }
    {
    }

    /**
     * Constructor that accepts a task name.
     *
     * @note This is not a Tracker per se; this is syntactic sugar to name a task as it is created.
     * @tparam T A type that is convertible to `const char*`.
     * @param name Either a `const char*` (dynamic string) or a `const char (&)[N]` (literal string) as the string name
     *     for a task.
     * @see ITasking::nameTask()
     */
    template <class T, std::enable_if_t<std::is_convertible<T, const char*>::value, bool> = false>
    constexpr Tracker(T&& name)
        : Object{ detail::is_literal_string<T>::value ? ObjectType::eTaskNameLiteral : ObjectType::eTaskName,
                  const_cast<void*>(reinterpret_cast<const void*>(name)) }
    {
    }

    /**
     * Constructor that accepts a Future<void>&. The Future will be initialized before the task can begin.
     */
    Tracker(Future<>& fut) : Object{ ObjectType::ePtrTaskContext, fut.ptask() }
    {
    }

    /**
     * Constructor that accepts a Future<void>*. The Future<void> will be initialized before the task can begin.
     * The Future<void> pointer can be `nullptr`.
     */
    Tracker(Future<>* fut) : Object{ ObjectType::ePtrTaskContext, fut ? fut->ptask() : nullptr }
    {
    }

    /**
     * Constructor that accepts a SharedFuture<void>&. The SharedFuture will be initialized before the task can begin.
     */
    Tracker(SharedFuture<>& fut) : Object{ ObjectType::ePtrTaskContext, fut.ptask() }
    {
    }

    /**
     * Constructor that accepts a SharedFuture<void>*. The SharedFuture<void> will be initialized before the task can
     * begin. The SharedFuture<void> pointer can be `nullptr`.
     */
    Tracker(SharedFuture<>* fut) : Object{ ObjectType::ePtrTaskContext, fut ? fut->ptask() : nullptr }
    {
    }

    /**
     * Constructor that accepts a TaskContext&. The value will be atomically written before the task can begin.
     */
    constexpr Tracker(TaskContext& ctx) : Object{ ObjectType::ePtrTaskContext, &ctx }
    {
    }

    /**
     * Constructor that accepts a TaskContext*. The value will be atomically written before the task can begin.
     * The TaskContext* can be `nullptr`.
     */
    constexpr Tracker(TaskContext* ctx) : Object{ ObjectType::ePtrTaskContext, ctx }
    {
    }

    /**
     * Constructor that accepts a TaskGroup&. The TaskGroup will be entered immediately and left when the task finishes.
     * The TaskGroup must exist until the task completes.
     */
    Tracker(TaskGroup& grp);

    /**
     * Constructor that accepts a TaskGroup*. The TaskGroup will be entered immediately and left when the task finishes.
     * The TaskGroup* can be `nullptr` in which case nothing happens. The TaskGroup must exist until the task completes.
     */
    Tracker(TaskGroup* grp);

private:
    friend struct Trackers;
};

/**
 * Helper class to ensure correct compliance with trackers parameter of ITasking::addTask() variants
 */
struct Trackers final
{
    /**
     * Constructor that accepts a single Tracker.
     *
     * @param t The type passed to the Tracker constructor.
     */
    template <class T, std::enable_if_t<std::is_constructible<Tracker, T>::value, bool> = false>
    constexpr Trackers(T&& t) : m_variant(Tracker(t))
    {
    }

    /**
     * Constructor that accepts an initializer_list of Tracker objects.
     *
     * @param il The `std::initializer_list` of Tracker objects.
     */
    constexpr Trackers(std::initializer_list<Tracker> il) : m_variant(carb::cpp17::in_place_index<1>)
    {
        if (il.size() == 1)
            m_variant.emplace<Tracker>(*il.begin());
        else
        {
            auto& vec = carb::cpp17::get<1>(m_variant);
            vec.reserve(il.size());
            vec.insert(vec.end(), il.begin(), il.end());
        }
    }

    /**
     * Constructor that accepts an initializer_list of Tracker objects and additional Tracker objects.
     *
     * @param il The `std::initializer_list` of Tracker objects.
     * @param p A pointer to additional Tracker objects; size specified by @p count.
     * @param count The number of additional Tracker objects in the list specified by @p p.
     */
    Trackers(std::initializer_list<Tracker> il, Tracker const* p, size_t count)
        : m_variant(carb::cpp17::in_place_index<1>)
    {
        if ((il.size() + count) == 1)
        {
            m_variant.emplace<Tracker>(il.size() == 0 ? *p : *il.begin());
        }
        else
        {
            auto& vec = carb::cpp17::get<1>(m_variant);
            vec.reserve(il.size() + count);
            vec.insert(vec.end(), il.begin(), il.end());
            vec.insert(vec.end(), p, p + count);
        }
    }

    /**
     * Retrieves a list of Tracker objects managed by this helper object.
     *
     * @param trackers Receives a pointer to a list of Tracker objects.
     * @param count Receives the count of Tracker objects.
     */
    void output(Tracker const*& trackers, size_t& count) const
    {
        static_assert(sizeof(Object) == sizeof(Tracker), "");
        fill(reinterpret_cast<Object const*&>(trackers), count);
    }

    CARB_PREVENT_COPY(Trackers);

    /**
     * Trackers is move-constructible.
     */
    Trackers(Trackers&&) = default;
    /**
     * Trackers is move-assignable.
     */
    Trackers& operator=(Trackers&&) = default;

private:
    friend struct ITasking;
    using Variant = carb::cpp17::variant<Tracker, std::vector<Tracker>>;
    Variant m_variant;
    Counter* fill(carb::tasking::Object const*& trackers, size_t& count) const
    {
        if (auto* vec = carb::cpp17::get_if<1>(&m_variant))
        {
            trackers = vec->data();
            count = vec->size();
        }
        else
        {
            const Tracker& t = carb::cpp17::get<0>(m_variant);
            trackers = &t;
            count = 1;
        }
        return detail::kListOfCounters;
    }
};

//! A macro that can be used to mark a function as async, that is, it always executes in the context of a task.
//!
//! Generally the body of the function has one of @ref CARB_ASSERT_ASYNC, @ref CARB_CHECK_ASYNC, or
//! @ref CARB_FATAL_UNLESS_ASYNC.
//!
//! @code{.cpp}
//! void CARB_ASYNC Context::loadTask();
//! @endcode
#define CARB_ASYNC

//! A macro that can be used to mark a function as possibly async, that is, it may execute in the context of a task.
//! @code{.cpp}
//! void CARB_MAYBE_ASYNC Context::loadTask();
//! @endcode
#define CARB_MAYBE_ASYNC

//! Helper macro that results in a boolean expression which is `true` if the current thread is running in task context.
#define CARB_IS_ASYNC                                                                                                  \
    (::carb::getCachedInterface<carb::tasking::ITasking>()->getTaskContext() != ::carb::tasking::kInvalidTaskContext)

//! A macro that is used to assert that a scope is running in task context in debug builds only.
#define CARB_ASSERT_ASYNC CARB_ASSERT(CARB_IS_ASYNC)

//! A macro that is used to assert that a scope is running in task context in debug and checked builds.
#define CARB_CHECK_ASYNC CARB_CHECK(CARB_IS_ASYNC)

//! A macro that is used to assert that a scope is running in task context.
#define CARB_FATAL_UNLESS_ASYNC CARB_FATAL_UNLESS(CARB_IS_ASYNC, "Not running in task context!")

} // namespace tasking
} // namespace carb
