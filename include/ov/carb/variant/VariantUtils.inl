// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

namespace carb
{
namespace variant
{

template <class Type, class TranslatorType>
VariantData internalTranslate(Type&& type, TranslatorType&& t)
{
    auto iface = getCachedInterface<IVariant>();
    CARB_ASSERT(iface, "Missing required interface: IVariant");
    return { iface->getVTable(t.type()), t.data(std::forward<Type>(type)) };
}

template <class Type>
VariantData translate(Type&& type) noexcept
{
    // NOTE: If you get an error on this line about "use of undefined type<...>" it means that there is no
    // Translator<> for the type you're trying to convert to. See the comment block for Translator in VariantTypes.h.
    // The Translator structs themselves are in Translator.inl.
    return internalTranslate(std::forward<Type>(type), Translator<std::decay_t<Type>>{});
}

inline void traits::swap(VariantData& lhs, VariantData& rhs) noexcept
{
    std::swap(lhs, rhs);
}

inline void traits::destruct(VariantData& self) noexcept
{
    if (self.vtable && self.vtable->Destructor)
        self.vtable->Destructor(&self);
    self = {};
}

inline VariantData traits::copy(const VariantData& self) noexcept
{
    if (self.vtable && self.vtable->Copy)
    {
        VariantData vd = self.vtable->Copy(&self);
        CARB_ASSERT(vd.vtable == self.vtable, "v-table %s does not match expected type %s!",
                    vd.vtable->typeName.c_str(), self.vtable->typeName.c_str());
        return vd;
    }
    return self;
}

inline bool traits::equals(const VariantData& self, const VariantData& other) noexcept
{
    if (self.vtable && self.vtable->Equals)
        return self.vtable->Equals(&self, &other);
    return std::memcmp(&self, &other, sizeof(VariantData)) == 0;
}

inline omni::string traits::toString(const VariantData& self) noexcept
{
    if (self.vtable)
    {
        if (self.vtable->ToString)
            return self.vtable->ToString(&self);
        else
            return omni::string(omni::formatted, "%p:%p", self.vtable, self.data);
    }
    return {};
}

inline bool traits::convertTo(const VariantData& self, const VTable* newType, VariantData& out) noexcept
{
    if (self.vtable && newType)
    {
        if (self.vtable == newType)
            return (out = traits::copy(self), true);
        if (self.vtable->ConvertTo)
        {
            bool b = self.vtable->ConvertTo(&self, newType, &out);
            CARB_ASSERT(!b || out.vtable == newType, "v-table %s doesn't match requested type %s!",
                        out.vtable->typeName.c_str(), newType->typeName.c_str());
            return b;
        }
    }
    return false;
}

inline size_t traits::hash(const VariantData& self) noexcept
{
    return self.vtable && self.vtable->Hash ? self.vtable->Hash(&self) : size_t(self.data);
}

inline Variant::Variant() noexcept : VariantData{}
{
}

inline Variant::~Variant() noexcept
{
    traits::destruct(data());
}

inline Variant::Variant(const Variant& other) noexcept
{
    data() = traits::copy(other);
}

inline Variant& Variant::operator=(const Variant& other) noexcept
{
    if (this != &other)
    {
        traits::destruct(data());
        data() = traits::copy(other);
    }
    return *this;
}

inline Variant::Variant(Variant&& other) noexcept : VariantData{}
{
    traits::swap(data(), other.data());
}

inline Variant& Variant::operator=(Variant&& other) noexcept
{
    traits::swap(data(), other.data());
    return *this;
}

inline bool Variant::operator==(const Variant& other) const noexcept
{
    return traits::equals(data(), other.data());
}

inline bool Variant::operator!=(const Variant& other) const noexcept
{
    return !(*this == other);
}

inline bool Variant::hasValue() const noexcept
{
    return data().vtable != nullptr;
}

inline omni::string Variant::toString() const noexcept
{
    return traits::toString(data());
}

inline size_t Variant::getHash() const noexcept
{
    return traits::hash(data());
}

template <class T>
cpp17::optional<T> Variant::getValue() const noexcept
{
    Translator<std::decay_t<T>> t{};

    // If the type matches exactly, we can interpret the data.
    if (data().vtable && t.type() == data().vtable->typeName)
        return t.value(data().data);

    VariantData temp;
    auto iface = getCachedInterface<IVariant>();
    CARB_ASSERT(iface, "Failed to acquire interface IVariant");
    if (traits::convertTo(data(), iface->getVTable(t.type()), temp))
    {
        CARB_SCOPE_EXIT
        {
            traits::destruct(temp);
        };
        return t.value(temp.data);
    }

    return {};
}

template <class T>
T Variant::getValueOr(T&& fallback) const noexcept
{
    return getValue<T>().value_or(std::forward<T>(fallback));
}

template <class T>
Variant Variant::convertTo() const noexcept
{
    Variant other;
    Translator<std::decay_t<T>> t{};
    VariantData temp;
    auto iface = getCachedInterface<IVariant>();
    CARB_ASSERT(iface, "Failed to acquire interface IVariant");
    if (traits::convertTo(data(), iface->getVTable(t.type()), temp))
    {
        Variant& v = static_cast<Variant&>(temp);
        return v;
    }
    return {};
}

constexpr Registrar::Registrar() noexcept : m_type{}
{
}

inline Registrar::Registrar(const VTable* vtable) noexcept
{
    CARB_ASSERT(vtable); // Null v-table not allowed.
    auto iface = getCachedInterface<IVariant>();
    CARB_ASSERT(iface, "Failed to acquire interface IVariant");
    if (iface->registerType(vtable))
        m_type = vtable->typeName;
}

inline Registrar::~Registrar() noexcept
{
    reset();
}

inline Registrar::Registrar(Registrar&& other) noexcept : m_type(std::exchange(other.m_type, {}))
{
}

inline Registrar& Registrar::operator=(Registrar&& other) noexcept
{
    std::swap(m_type, other.m_type);
    return *this;
}

inline bool Registrar::isEmpty() const noexcept
{
    return m_type.isEmpty();
}

inline RString Registrar::getType() const noexcept
{
    return m_type;
}

inline void Registrar::reset() noexcept
{
    auto type = std::exchange(m_type, {});
    if (!type.isEmpty())
    {
        auto iface = getCachedInterface<IVariant>();
        if (iface)
        {
            iface->unregisterType(type);
        }
    }
}

} // namespace variant
} // namespace carb
