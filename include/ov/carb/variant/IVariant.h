// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Interface definition for *carb.variant.plugin*
#pragma once

#include "../Interface.h"
#include "VariantTypes.h"

namespace carb
{

//! Namespace for *carb.variant.plugin* and related utilities.
namespace variant
{

//! Interface for *carb.variant.plugin*
struct IVariant
{
    CARB_PLUGIN_INTERFACE("carb::variant::IVariant", 0, 1)

    /**
     * Retrieves a v-table by variant type. Typically not used; see \ref Translator instead.
     * @param type The type to translate.
     * @returns A v-table for the given type, or \c nullptr in case of an unknown type.
     */
    const VTable*(CARB_ABI* getVTable)(RString type) noexcept;

    /**
     * Registers a user variant type.
     *
     * @note `vtable->typeName` must be a unique name within the running process and may not match any of the built-in
     * type names.
     * @param vtable Must not be \c nullptr. This pointer is retained by *carb.variant.plugin* so the caller must
     * guarantee its lifetime until it is unregistered with \ref unregisterType(). A program that still references the
     * v-table in a \ref VariantData (or \ref Variant) after it is unregistered is malformed.
     * @returns \c true if `vtable->typeName` was available and the type was successfully registered; \c false
     * otherwise.
     */
    bool(CARB_ABI* registerType)(const VTable* vtable) noexcept;

    /**
     * Registers a user variant type.
     *
     * @param type The name of the type previously registered with \ref registerType().
     * @returns \c true if the type was unregistered; \c false otherwise (i.e. the type was not previously registered).
     */
    bool(CARB_ABI* unregisterType)(RString type) noexcept;

    //! @private
    VariantArray*(CARB_ABI* internalCreateArray)(const Variant* p, size_t count);

    /**
     * Creates a \ref VariantArray object from the given parameters.
     *
     * @param p The raw array to copy into the new \ref VariantArray; may be \c nullptr to produce an empty array.
     * @param count The count of items in @p p.
     * @returns A newly created \ref VariantArray object.
     */
    CARB_NODISCARD VariantArrayPtr createArray(const Variant* p = nullptr, size_t count = 0);

    //! @private
    VariantMap*(CARB_ABI* internalCreateMap)();

    /**
     * Creates a \ref VariantMap object.
     * @returns A newly created \ref VariantMap object.
     */
    CARB_NODISCARD VariantMapPtr createMap();
};

} // namespace variant
} // namespace carb

#include "IVariant.inl"
