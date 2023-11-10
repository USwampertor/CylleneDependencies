// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Translator definitions for *carb.variant.plugin*
#pragma once

#include "IVariant.h"
#include "../InterfaceUtils.h"
#include "../Strong.h"

namespace carb
{
namespace dictionary
{
struct Item;
}

namespace variant
{

template <>
struct Translator<std::nullptr_t, void>
{
    RString type() const noexcept
    {
        return eNull;
    }
    void* data(std::nullptr_t) const noexcept
    {
        return nullptr;
    }
    std::nullptr_t value(void*) const noexcept
    {
        return nullptr;
    }
};

template <class T>
struct Translator<T,
                  typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<bool, T>::value &&
                                            (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) &&
                                            sizeof(T) <= sizeof(void*)>>
{
    constexpr RString type() const noexcept
    {
        constexpr bool s = std::is_signed<T>::value;
        switch (sizeof(T))
        {
            case 1:
                return s ? eInt8 : eUInt8;
            case 2:
                return s ? eInt16 : eUInt16;
            case 4:
                return s ? eInt32 : eUInt32;
            case 8:
                return s ? eInt64 : eUInt64;
        }
        CARB_ASSERT(false); // Should never get here
        return {};
    }
    void* data(T val) const noexcept
    {
        void* p{};
        memcpy(&p, &val, sizeof(T));
        return p;
    }
    T value(void* val) const noexcept
    {
        T t;
        memcpy(&t, &val, sizeof(T));
        return t;
    }
};

template <class T>
struct Translator<T,
                  typename std::enable_if_t<std::is_floating_point<T>::value && (sizeof(T) == 4 || sizeof(T) == 8) &&
                                            sizeof(T) <= sizeof(void*)>>
{
    RString type() const noexcept
    {
        return sizeof(T) == 4 ? eFloat : eDouble;
    }
    void* data(T val) const noexcept
    {
        void* p{};
        memcpy(&p, &val, sizeof(T));
        return p;
    }
    T value(void* val) const noexcept
    {
        T t;
        memcpy(&t, &val, sizeof(T));
        return t;
    }
};

template <>
struct Translator<omni::string, void>
{
    RString type() const noexcept
    {
        return eString;
    }
    void* data(omni::string str) const noexcept
    {
        return new (carb::allocate(sizeof(omni::string))) omni::string(std::move(str));
    }
    const omni::string& value(void* val) const noexcept
    {
        return *static_cast<const omni::string*>(val);
    }
};

template <>
struct Translator<bool, void>
{
    RString type() const noexcept
    {
        return eBool;
    }
    void* data(bool b) const noexcept
    {
        void* p{};
        memcpy(&p, &b, sizeof(b));
        return p;
    }
    bool value(void* val) const noexcept
    {
        bool b;
        memcpy(&b, &val, sizeof(b));
        return b;
    }
};

// Helper that is true_type if pointer-type T is pointer-type U excluding const/volatile
// Used to enable types for both const/volatile and non-const/volatile
template <class T, class U>
struct IsCVOrNot
    : std::integral_constant<bool,
                             std::is_pointer<T>::value &&
                                 std::is_same<std::remove_cv_t<std::remove_pointer_t<T>>, std::remove_pointer_t<U>>::value>
{
};

// [const] char*
template <class T>
struct Translator<T, typename std::enable_if_t<IsCVOrNot<T, char*>::value>>
{
    RString type() const noexcept
    {
        return eCharPtr;
    }
    void* data(T p) const noexcept
    {
        return (void*)p;
    }
    const char* value(void* val) const noexcept
    {
        return static_cast<const char*>(val);
    }
};

// [const] dictionary::Item*
template <class T>
struct Translator<T, typename std::enable_if_t<IsCVOrNot<T, dictionary::Item*>::value>>
{
    RString type() const noexcept
    {
        return eDictionary;
    }
    void* data(T p) const noexcept
    {
        return (void*)p;
    }
    const dictionary::Item* value(void* p) const noexcept
    {
        return static_cast<const dictionary::Item*>(p);
    }
};

template <class T, class U>
struct Translator<Strong<T, U>, void> : public Translator<T>
{
    using Parent = Translator<T>;
    void* data(Strong<T, U> val) const noexcept
    {
        return Parent::data(val.get());
    }
    const Strong<T, U> value(void* p) const noexcept
    {
        return Strong<T, U>{ Parent::value(p) };
    }
};

template <>
struct Translator<VariantArrayPtr, void>
{
    RString type() const noexcept
    {
        return eVariantArray;
    }
    void* data(VariantArrayPtr p) const noexcept
    {
        // Keep the ref-count that `p` held by detaching the smart pointer.
        return p.detach();
    }
    VariantArrayPtr value(void* p) const noexcept
    {
        return VariantArrayPtr(static_cast<VariantArray*>(p));
    }
};

// [const] VariantArray*
template <class T>
struct Translator<T, typename std::enable_if_t<IsCVOrNot<T, VariantArray*>::value>>
{
    RString type() const noexcept
    {
        return eVariantArray;
    }
    void* data(VariantArray* p) const noexcept // IObject requires non-const
    {
        if (p)
            p->addRef();
        return p;
    }
    T value(void* p) const noexcept
    {
        return static_cast<VariantArray*>(p);
    }
};

template <>
struct Translator<std::pair<Variant, Variant>, void>
{
    RString type() const noexcept
    {
        return eVariantPair;
    }
    void* data(std::pair<Variant, Variant> pair) const noexcept
    {
        void* mem = carb::allocate(sizeof(VariantPair));
        return new (mem) VariantPair{ std::move(pair.first), std::move(pair.second) };
    }
    std::pair<Variant, Variant> value(void* p) const noexcept
    {
        auto pair = static_cast<VariantPair*>(p);
        return std::make_pair(pair->first, pair->second);
    }
};

template <bool Uncased, class Base>
struct Translator<carb::detail::RStringTraits<Uncased, Base>, void>
{
    constexpr static bool IsKey = std::is_same<Base, carb::detail::RStringKeyBase>::value;
    using RStringType = typename std::conditional_t<Uncased,
                                                    std::conditional_t<IsKey, RStringUKey, RStringU>,
                                                    std::conditional_t<IsKey, RStringKey, RString>>;
    static_assert(sizeof(RStringType) <= sizeof(void*), "RStringType must fit within a void*");

    RString type() const noexcept
    {
        if (IsKey)
            return Uncased ? eRStringUKey : eRStringKey;
        return Uncased ? eRStringU : eRString;
    }
    void* data(const RStringType& rstring) const noexcept
    {
        union
        {
            RStringType in;
            void* out;
        } u{ rstring };
        return u.out;
    }
    RStringType value(void* p) const noexcept
    {
        union
        {
            void* in;
            RStringType out;
        } u{ p };
        return u.out;
    }
};

template <class T>
struct Translator<T,
                  std::enable_if_t<cpp17::disjunction<typename std::is_same<RString, T>,
                                                      typename std::is_same<RStringU, T>,
                                                      typename std::is_same<RStringKey, T>,
                                                      typename std::is_same<RStringUKey, T>>::value>>
    : public Translator<carb::detail::RStringTraits<
          T::IsUncased,
          typename std::conditional_t<
              cpp17::disjunction<typename std::is_same<RStringKey, T>, typename std::is_same<RStringUKey, T>>::value,
              carb::detail::RStringKeyBase,
              carb::detail::RStringBase>>>
{
};

template <>
struct Translator<VariantMapPtr, void>
{
    RString type() const noexcept
    {
        return eVariantMap;
    }
    void* data(VariantMapPtr p) const noexcept
    {
        // Keep the ref-count that `p` held by detaching the smart pointer.
        return p.detach();
    }
    VariantMapPtr value(void* p) const noexcept
    {
        return VariantMapPtr(static_cast<VariantMap*>(p));
    }
};

// [const] VariantMap*
template <class T>
struct Translator<T, typename std::enable_if_t<IsCVOrNot<T, VariantMap*>::value>>
{
    RString type() const noexcept
    {
        return eVariantMap;
    }
    void* data(VariantMap* p) const noexcept // IObject requires non-const
    {
        if (p)
            p->addRef();
        return p;
    }
    T value(void* p) const noexcept
    {
        return static_cast<VariantMap*>(p);
    }
};

// NOTE: If you add a new Translator here, make sure that it is documented in the comment block for Translator in
// VariantTypes.h

} // namespace variant
} // namespace carb
