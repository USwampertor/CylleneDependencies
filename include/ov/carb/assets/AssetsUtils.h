// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Utilities for *carb.assets.plugin*
#pragma once

#include "IAssets.h"

namespace carb
{

namespace assets
{

/**
 * A RAII-style helper class to manage the result of \ref IAssets::acquireSnapshot().
 * `operator bool()` can be used to test if the asset successfully acquired.
 * `getReason()` can be used to check why an asset failed to load.
 * If the asset successfully loaded, it can be obtained with `get()`.
 */
template <class Type>
class ScopedSnapshot
{
public:
    /**
     * Default Constructor; produces an empty object
     */
    ScopedSnapshot() = default;

    /**
     * \c nullptr Constructor; produces an empty object
     */
    ScopedSnapshot(std::nullptr_t)
    {
    }

    /**
     * Constructs a \c ScopedSnapshot for the given asset ID.
     *
     * If snapshot acquisition fails, `*this` will be \c false; use \ref getReason() to determine why.
     * @param assets The IAssets interface
     * @param assetId The asset ID to acquire a snapshot for.
     */
    ScopedSnapshot(IAssets* assets, Id assetId) : m_assets(assets)
    {
        m_snapshot = assets->acquireSnapshot(assetId, getAssetType<Type>(), m_reason);
        m_value = reinterpret_cast<Type*>(assets->getDataFromSnapshot(m_snapshot));
    }

    //! Destructor
    ~ScopedSnapshot()
    {
        release();
    }

    //! ScopedSnapshot is move-constructible.
    //! @param other The other \c ScopedSnapshot to move from; \c other will be empty.
    ScopedSnapshot(ScopedSnapshot&& other)
    {
        m_value = other.m_value;
        m_assets = other.m_assets;
        m_snapshot = other.m_snapshot;
        m_reason = other.m_reason;
        other.m_assets = nullptr;
        other.m_value = nullptr;
        other.m_snapshot = kInvalidSnapshot;
        other.m_reason = Reason::eFailed;
    }

    /**
     * Move-assignment operator.
     * @param other The other \c ScopedSnapshot to move from; \c other will be empty.
     * @returns \c *this
     */
    ScopedSnapshot& operator=(ScopedSnapshot&& other)
    {
        // This assert should never happen, but it is possible to accidentally write this
        // code, though one has to contort themselves to do it. It is considered
        // invalid nonetheless.
        CARB_ASSERT(this != &other);
        release();
        m_value = other.m_value;
        m_assets = other.m_assets;
        m_snapshot = other.m_snapshot;
        m_reason = other.m_reason;
        other.m_assets = nullptr;
        other.m_value = nullptr;
        other.m_snapshot = kInvalidSnapshot;
        other.m_reason = Reason::eFailed;
        return *this;
    }

    CARB_PREVENT_COPY(ScopedSnapshot);

    /**
     * Obtain the asset data from the snapshot.
     * @returns The loaded asset if the asset load was successful; \c nullptr otherwise.
     */
    Type* get()
    {
        return m_value;
    }

    //! @copydoc get()
    const Type* get() const
    {
        return m_value;
    }

    /**
     * Dereference-access operator.
     * @returns The loaded asset; malformed if `*this == false`.
     */
    Type* operator->()
    {
        return get();
    }

    //! @copydoc operator->()
    const Type* operator->() const
    {
        return get();
    }

    /**
     * Dereference operator.
     * @returns A reference to the loaded asset; malformed if `*this == false`.
     */
    Type& operator*()
    {
        return *get();
    }

    //! @copydoc operator*()
    const Type& operator*() const
    {
        return *get();
    }

    /**
     * Test if the asset snapshot successfully loaded.
     * @returns \c true if the asset snapshot successfully loaded and its value can be retrieved via \ref get();
     * \c false otherwise.
     */
    constexpr explicit operator bool() const noexcept
    {
        return m_value != nullptr;
    }

    /**
     * Obtain the current asset status.
     * @returns the \ref Reason status code based on acquiring the snapshot. An empty \c ScopedSnapshot will return
     * \ref Reason::eFailed.
     */
    Reason getReason() const
    {
        return m_reason;
    }

private:
    void release()
    {
        if (m_assets && m_snapshot)
        {
            m_assets->releaseSnapshot(m_snapshot);
        }
        m_value = nullptr;
        m_assets = nullptr;
        m_snapshot = kInvalidSnapshot;
        m_reason = Reason::eFailed;
    }

    // Note this member is first to help in debugging.
    Type* m_value = nullptr;
    carb::assets::IAssets* m_assets = nullptr;
    Snapshot m_snapshot = kInvalidSnapshot;
    Reason m_reason = Reason::eFailed;
};

//! \c ScopedSnapshot equality operator
//! @param a A \c ScopedSnapshot to compare
//! @param b A \c ScopedSnapshot to compare
//! @returns \c true if \c a and \c b are equal; \c false otherwise.
template <class Type>
bool operator==(const carb::assets::ScopedSnapshot<Type>& a, const carb::assets::ScopedSnapshot<Type>& b)
{
    return a.get() == b.get();
}

//! \c ScopedSnapshot inequality operator
//! @param a A \c ScopedSnapshot to compare
//! @param b A \c ScopedSnapshot to compare
//! @returns \c true if \c a and \c b are unequal; \c false otherwise.
template <class Type>
bool operator!=(const carb::assets::ScopedSnapshot<Type>& a, const carb::assets::ScopedSnapshot<Type>& b)
{
    return a.get() != b.get();
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template <class Type>
bool operator==(const carb::assets::ScopedSnapshot<Type>& a, std::nullptr_t)
{
    return a.get() == nullptr;
}

template <class Type>
bool operator==(std::nullptr_t, const carb::assets::ScopedSnapshot<Type>& a)
{
    return a.get() == nullptr;
}

template <class Type>
bool operator!=(const carb::assets::ScopedSnapshot<Type>& a, std::nullptr_t)
{
    return a.get() != nullptr;
}

template <class Type>
bool operator!=(std::nullptr_t, const carb::assets::ScopedSnapshot<Type>& a)
{
    return a.get() != nullptr;
}
#endif

} // namespace assets
} // namespace carb
