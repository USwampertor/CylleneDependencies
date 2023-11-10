// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#include "RStringInternals.inl"

namespace carb
{

namespace detail
{

// Disable warnings since the Base{} initializers might not include every field, which is non-trivial since the Base
// is a template parameter and some bases have different fields. However, the later fields will be zero-initialized.
CARB_IGNOREWARNING_GNUC_WITH_PUSH("-Wmissing-field-initializers")

template <bool Uncased, class Base>
inline constexpr RStringTraits<Uncased, Base>::RStringTraits() noexcept : Base{ 0, Uncased }
{
}

template <bool Uncased, class Base>
inline constexpr RStringTraits<Uncased, Base>::RStringTraits(eRString staticString) noexcept
    : Base{ uint32_t(staticString), Uncased }
{
    CARB_ASSERT(uint32_t(staticString) <= kMaxStaticRString);
}

template <bool Uncased, class Base>
inline RStringTraits<Uncased, Base>::RStringTraits(const char* str, RStringOp op)
    : Base{ rstring::Internals::get().findOrAdd(str, Uncased, op), Uncased }
{
}

template <bool Uncased, class Base>
inline RStringTraits<Uncased, Base>::RStringTraits(const char* str, size_t len, RStringOp op)
    : Base{ rstring::Internals::get().findOrAdd(str, len, Uncased, op), Uncased }
{
}

template <bool Uncased, class Base>
inline RStringTraits<Uncased, Base>::RStringTraits(const std::string& str, RStringOp op)
    : Base{ rstring::Internals::get().findOrAdd(str.data(), str.length(), Uncased, op), Uncased }
{
}

template <bool Uncased, class Base>
inline RStringTraits<Uncased, Base>::RStringTraits(uint32_t stringId) noexcept : Base{ stringId, Uncased }
{
    // If we're uncased, we should be referencing an authority.
    CARB_ASSERT(!Uncased || rstring::Internals::get()[stringId]->m_authority);
}

CARB_IGNOREWARNING_GNUC_POP

template <bool Uncased, class Base>
inline bool RStringTraits<Uncased, Base>::isValid() const noexcept
{
    return rstring::Internals::get()[this->m_stringId] != nullptr;
}

template <bool Uncased, class Base>
inline constexpr bool RStringTraits<Uncased, Base>::isEmpty() const noexcept
{
    return this->m_stringId == 0;
}

template <bool Uncased, class Base>
inline constexpr bool RStringTraits<Uncased, Base>::isUncased() const noexcept
{
    CARB_ASSERT(this->m_uncased == Uncased);
    return Uncased;
}

template <bool Uncased, class Base>
inline constexpr uint32_t RStringTraits<Uncased, Base>::getStringId() const noexcept
{
    return this->m_stringId;
}

template <bool Uncased, class Base>
inline size_t RStringTraits<Uncased, Base>::getHash() const
{
    auto& internals = rstring::Internals::get();
    CARB_ASSERT(this->m_uncased == Uncased);
    return Uncased ? internals[this->m_stringId]->m_uncasedHash : internals.getHash(this->m_stringId);
}

template <bool Uncased, class Base>
inline size_t RStringTraits<Uncased, Base>::getUncasedHash() const noexcept
{
    return rstring::Internals::get()[this->m_stringId]->m_uncasedHash;
}

template <bool Uncased, class Base>
inline const char* RStringTraits<Uncased, Base>::c_str() const noexcept
{
    return rstring::Internals::get()[this->m_stringId]->m_string;
}

template <bool Uncased, class Base>
inline const char* RStringTraits<Uncased, Base>::data() const noexcept
{
    return rstring::Internals::get()[this->m_stringId]->m_string;
}

template <bool Uncased, class Base>
inline size_t RStringTraits<Uncased, Base>::length() const noexcept
{
    return rstring::Internals::get()[this->m_stringId]->m_stringLen;
}

template <bool Uncased, class Base>
inline std::string RStringTraits<Uncased, Base>::toString() const
{
    const carb::detail::rstring::Rec* rec = carb::detail::rstring::Internals::get()[this->m_stringId];
    return std::string(rec->m_string, rec->m_stringLen);
}

template <bool Uncased, class Base>
inline bool RStringTraits<Uncased, Base>::operator==(const RStringTraits<Uncased, Base>& other) const
{
    CARB_ASSERT(this->m_uncased == Uncased && other.m_uncased == Uncased);
    return other.m_stringId == this->m_stringId;
}

template <bool Uncased, class Base>
inline bool RStringTraits<Uncased, Base>::operator!=(const RStringTraits<Uncased, Base>& other) const
{
    return !(*this == other);
}

template <bool Uncased, class Base>
inline bool RStringTraits<Uncased, Base>::owner_before(const RStringTraits<Uncased, Base>& other) const
{
    CARB_ASSERT(this->m_uncased == Uncased && other.m_uncased == Uncased);
    return this->m_stringId < other.m_stringId;
}

template <bool Uncased, class Base>
template <bool OtherUncased, class OtherBase>
inline int RStringTraits<Uncased, Base>::compare(const RStringTraits<OtherUncased, OtherBase>& other) const
{
    CARB_ASSERT(Uncased == this->m_uncased);
    CARB_ASSERT(OtherUncased == other.isUncased());
    return !(Uncased | OtherUncased) ? rstring::casedCompare(c_str(), length(), other.c_str(), other.length()) :
                                       rstring::uncasedCompare(c_str(), length(), other.c_str(), other.length());
}

template <bool Uncased, class Base>
inline int RStringTraits<Uncased, Base>::compare(const char* s) const
{
    CARB_ASSERT(Uncased == this->m_uncased);
    return !Uncased ? rstring::casedCompare(c_str(), length(), s, std::strlen(s)) :
                      rstring::uncasedCompare(c_str(), length(), s, std::strlen(s));
}

template <bool Uncased, class Base>
inline int RStringTraits<Uncased, Base>::compare(size_t pos, size_t count, const char* s) const
{
    return compare(pos, count, s, std::strlen(s));
}

namespace
{
inline int checkCased(char c1, char c2)
{
    return int(c1) - int(c2);
}

inline int checkUncased(char c1, char c2)
{
    return checkCased(char(std::tolower(c1)), char(std::tolower(c2)));
}
} // namespace

template <bool Uncased, class Base>
inline int RStringTraits<Uncased, Base>::compare(size_t pos, size_t count, const char* s, size_t len) const
{
    const rstring::Rec* rec = rstring::Internals::get()[this->m_stringId];
    CARB_ASSERT(pos <= rec->m_stringLen);
    // Take the smallest of count, len, or remaining string after pos
    count = ::carb_min(count, rec->m_stringLen - pos);
    size_t remain = ::carb_min(count, len);

    CARB_ASSERT(this->m_uncased == Uncased);
    auto check = !Uncased ? checkCased : checkUncased;
    const char* my = rec->m_string + pos;
    for (; remain != 0; --remain, ++my, ++s)
    {
        int c = check(*my, *s);
        if (c != 0)
            return c;
    }
    // Otherwise equal, so whichever is longer is ordered later.
    return int(ptrdiff_t(count - len));
}

template <bool Uncased, class Base>
inline int RStringTraits<Uncased, Base>::compare(const std::string& s) const
{
    CARB_ASSERT(this->m_uncased == Uncased);
    return !Uncased ? rstring::casedCompare(c_str(), length(), s.c_str(), s.length()) :
                      rstring::uncasedCompare(c_str(), length(), s.c_str(), s.length());
}

template <bool Uncased, class Base>
inline int RStringTraits<Uncased, Base>::compare(size_t pos, size_t count, const std::string& s) const
{
    return compare(pos, count, s.c_str(), s.length());
}

} // namespace detail

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RString functions

inline constexpr RString::RString() noexcept
{
}

inline constexpr RString::RString(eRString staticString) noexcept : Base(staticString)
{
}

inline RString::RString(const char* str, RStringOp op) : Base(str, op)
{
}

inline RString::RString(const char* str, size_t len, RStringOp op) : Base(str, len, op)
{
}

inline RString::RString(const std::string& str, RStringOp op) : Base(str, op)
{
}

inline RString::RString(const RStringKey& other) noexcept : Base(other.getStringId())
{
}

inline RStringU RString::toUncased() const noexcept
{
    return RStringU(*this);
}

inline RString RString::truncate() const noexcept
{
    return *this;
}

inline RStringKey RString::toRStringKey(int32_t number) const
{
    return RStringKey(*this, number);
}

inline bool RString::operator==(const RString& other) const noexcept
{
    CARB_ASSERT(!m_uncased && !other.m_uncased);
    return m_stringId == other.m_stringId;
}

inline bool RString::operator!=(const RString& other) const noexcept
{
    return !(*this == other);
}

inline bool RString::owner_before(const RString& other) const noexcept
{
    CARB_ASSERT(!m_uncased && !other.m_uncased);
    return m_stringId < other.m_stringId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RStringU functions

inline constexpr RStringU::RStringU() noexcept : Base()
{
}

inline constexpr RStringU::RStringU(eRString staticString) noexcept : Base(staticString)
{
}

inline RStringU::RStringU(const char* str, RStringOp op) : Base(str, op)
{
}

inline RStringU::RStringU(const char* str, size_t len, RStringOp op) : Base(str, len, op)
{
}

inline RStringU::RStringU(const std::string& str, RStringOp op) : Base(str, op)
{
}

inline RStringU::RStringU(const RString& other)
    : Base(detail::rstring::Internals::get().convertUncased(other.getStringId()))
{
}

inline RStringU::RStringU(const RStringUKey& other) : Base(other.getStringId())
{
}

inline RStringU RStringU::toUncased() const noexcept
{
    return RStringU(*this);
}

inline RStringU RStringU::truncate() const noexcept
{
    return *this;
}

inline RStringUKey RStringU::toRStringKey(int32_t number) const
{
    return RStringUKey(*this, number);
}

inline bool RStringU::operator==(const RStringU& other) const noexcept
{
    CARB_ASSERT(m_uncased && other.m_uncased);
    return m_stringId == other.m_stringId;
}

inline bool RStringU::operator!=(const RStringU& other) const noexcept
{
    return !(*this == other);
}

inline bool RStringU::owner_before(const RStringU& other) const noexcept
{
    CARB_ASSERT(m_uncased && other.m_uncased);
    return m_stringId < other.m_stringId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RStringKey functions

inline constexpr RStringKey::RStringKey() noexcept : Base()
{
    CARB_ASSERT(this->m_number == 0);
}

inline constexpr RStringKey::RStringKey(eRString staticString, int32_t number) noexcept : Base(staticString)
{
    this->m_number = number;
}

inline RStringKey::RStringKey(const char* str, RStringOp op) : Base(str, op)
{
    CARB_ASSERT(this->m_number == 0);
}

inline RStringKey::RStringKey(int32_t number, const char* str, RStringOp op) : Base(str, op)
{
    this->m_number = number;
}

inline RStringKey::RStringKey(const char* str, size_t len, RStringOp op) : Base(str, len, op)
{
    CARB_ASSERT(this->m_number == 0);
}

inline RStringKey::RStringKey(int32_t number, const char* str, size_t len, RStringOp op) : Base(str, len, op)
{
    this->m_number = number;
}

inline RStringKey::RStringKey(const std::string& str, RStringOp op) : Base(str, op)
{
    CARB_ASSERT(this->m_number == 0);
}

inline RStringKey::RStringKey(int32_t number, const std::string& str, RStringOp op) : Base(str, op)
{
    this->m_number = number;
}

inline RStringKey::RStringKey(const RString& rstr, int32_t number) : Base(rstr.getStringId())
{
    this->m_number = number;
}

inline RStringUKey RStringKey::toUncased() const noexcept
{
    return RStringUKey(*this);
}

inline RString RStringKey::truncate() const noexcept
{
    return RString(*this);
}

inline bool RStringKey::operator==(const RStringKey& other) const noexcept
{
    CARB_ASSERT(!m_uncased && !other.m_uncased);
    return m_stringId == other.m_stringId && m_number == other.m_number;
}

inline bool RStringKey::operator!=(const RStringKey& other) const noexcept
{
    return !(*this == other);
}

inline bool RStringKey::owner_before(const RStringKey& other) const noexcept
{
    CARB_ASSERT(!m_uncased && !other.m_uncased);
    if (m_stringId != other.m_stringId)
        return m_stringId < other.m_stringId;
    return m_number < other.m_number;
}

inline size_t RStringKey::getHash() const
{
    auto hash = Base::getHash();
    return getNumber() ? carb::hashCombine(hash, getNumber()) : hash;
}

inline size_t RStringKey::getUncasedHash() const noexcept
{
    auto hash = Base::getUncasedHash();
    return getNumber() ? carb::hashCombine(hash, getNumber()) : hash;
}

inline std::string RStringKey::toString() const
{
    std::string str = Base::toString();
    if (m_number == 0)
        return str;

    str += '_';
    str += std::to_string(m_number);
    return str;
}

inline int32_t RStringKey::getNumber() const noexcept
{
    return m_number;
}

inline void RStringKey::setNumber(int32_t num) noexcept
{
    m_number = num;
}

inline int32_t& RStringKey::number() noexcept
{
    return m_number;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// RStringUKey functions

inline constexpr RStringUKey::RStringUKey() noexcept : Base()
{
    CARB_ASSERT(this->m_number == 0);
}

inline constexpr RStringUKey::RStringUKey(eRString staticString, int32_t number) noexcept : Base(staticString)
{
    this->m_number = number;
}

inline RStringUKey::RStringUKey(const char* str, RStringOp op) : Base(str, op)
{
    CARB_ASSERT(this->m_number == 0);
}

inline RStringUKey::RStringUKey(int32_t number, const char* str, RStringOp op) : Base(str, op)
{
    this->m_number = number;
}

inline RStringUKey::RStringUKey(const char* str, size_t len, RStringOp op) : Base(str, len, op)
{
    CARB_ASSERT(this->m_number == 0);
}

inline RStringUKey::RStringUKey(int32_t number, const char* str, size_t len, RStringOp op) : Base(str, len, op)
{
    this->m_number = number;
}

inline RStringUKey::RStringUKey(const std::string& str, RStringOp op) : Base(str, op)
{
    CARB_ASSERT(this->m_number == 0);
}

inline RStringUKey::RStringUKey(int32_t number, const std::string& str, RStringOp op) : Base(str, op)
{
    this->m_number = number;
}

inline RStringUKey::RStringUKey(const RStringU& rstr, int32_t number) : Base(rstr.getStringId())
{
    this->m_number = number;
}

inline RStringUKey::RStringUKey(const RStringKey& other)
    : Base(detail::rstring::Internals::get().convertUncased(other.getStringId()))
{
    this->m_number = other.getNumber();
}

inline RStringUKey RStringUKey::toUncased() const noexcept
{
    return RStringUKey(*this);
}

inline RStringU RStringUKey::truncate() const noexcept
{
    return RStringU(*this);
}

inline bool RStringUKey::operator==(const RStringUKey& other) const noexcept
{
    CARB_ASSERT(m_uncased && other.m_uncased);
    return m_stringId == other.m_stringId && m_number == other.m_number;
}

inline bool RStringUKey::operator!=(const RStringUKey& other) const noexcept
{
    return !(*this == other);
}

inline bool RStringUKey::owner_before(const RStringUKey& other) const noexcept
{
    CARB_ASSERT(m_uncased && other.m_uncased);
    if (m_stringId != other.m_stringId)
        return m_stringId < other.m_stringId;
    return m_number < other.m_number;
}

inline size_t RStringUKey::getHash() const
{
    auto hash = Base::getHash();
    return getNumber() ? carb::hashCombine(hash, getNumber()) : hash;
}

inline size_t RStringUKey::getUncasedHash() const noexcept
{
    auto hash = Base::getUncasedHash();
    return getNumber() ? carb::hashCombine(hash, getNumber()) : hash;
}

inline std::string RStringUKey::toString() const
{
    std::string str = Base::toString();
    if (m_number == 0)
        return str;

    str += '_';
    str += std::to_string(m_number);
    return str;
}

inline int32_t RStringUKey::getNumber() const noexcept
{
    return m_number;
}

inline void RStringUKey::setNumber(int32_t num) noexcept
{
    m_number = num;
}

inline int32_t& RStringUKey::number() noexcept
{
    return m_number;
}

} // namespace carb
