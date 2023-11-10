// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief A helper class to provide a resizable scratch buffer.
#pragma once

#include "../../carb/Defines.h"

#include <algorithm>
#include <iterator>

namespace omni
{
/** common namespace for extra helper functions and classes. */
namespace extras
{

/** A templated helper class to provide a simple resizable scratch buffer.  The buffer will only
 *  perform a dynamic allocation if the requested size requires it.  If only a small size is
 *  needed, a stack buffer will be used instead.  The intended usage pattern is to create the
 *  object, set its required size, then write to each item in the array as needed.  The buffer
 *  will not be initialized unless the contained type is a class that constructs itself.  It
 *  is left up to the caller to initialize the buffer's contents in all other cases.
 *
 *  The buffer will always occupy enough stack space to hold the larger of 512 bytes worth of
 *  the contained type, or 16 items of the contained type.  If the size is set beyond that
 *  limit, a new buffer will be allocated from the heap.  There is no tracking of how many
 *  items have been written to the buffer - that is left as an exercise for the caller.
 *
 *  @thread_safety There are no thread safety protections on this class.  This is intended
 *                 to be used as a local scratch buffer.  If potential concurrent access is
 *                 required, it is the caller's responsibility to protect access to this
 *                 object.
 *
 *  @tparam T           The data type that will be contained in the scratch buffer.  This
 *                      can be any primitive, struct, or class type.
 *  @tparam BaseSize_   The number of items of type @p T that can be held in the scratch
 *                      buffer's stack array without needing to allocate from the heap.
 *                      By default, this is guaranteed to be at least 16 items.
 *  @tparam ShrinkThreshold_    The threshold expressed as a percentage of the buffer's
 *                              previous size below which the buffer itself will be shrunken
 *                              on a resize operation that makes it smaller.  If the new
 *                              size of the buffer expressed as a percentage of the old size
 *                              is larger than this number, the previous buffer will be
 *                              kept instead of allocating and copying a new one.  This is
 *                              done as a performance optimization for callers that need
 *                              to grow and shrink their buffer frequently, but don't
 *                              necessarily want to incur the overhead of an allocation
 *                              and copy each time.  For example, if the threshold is 25%
 *                              and the buffer is resized from 20 items to 6, a reallocation
 *                              will not occur since the new size is 30% of the previous.
 *                              By contrast, if it is resized to 4 items from 20, a reallocation
 *                              will occur since the new size is 20% of the previous.  By default
 *                              the buffer will only grow and never shrink regardless of the
 *                              size and number of resize requests.
 */
#ifdef DOXYGEN_BUILD // Sphinx does not like the computation and reports errors, so special case for documentation.
template <typename T, size_t BaseSize_ = 16, size_t ShrinkThreshold_ = 100>
#else
template <typename T, size_t BaseSize_ = CARB_MAX(512ull / sizeof(T), 16ull), size_t ShrinkThreshold_ = 100>
#endif
class ScratchBuffer
{
public:
    /** The data type contained in this scratch buffer.  This can be any primitive, struct,
     *  or class type.  This is present here so that external callers can inspect the contained
     *  type if needed.
     */
    using DataType = T;

    /** The guaranteed base size of this scratch buffer in items.  This is present here so that
     *  external callers can inspect the guaranteed size of the buffer object.
     */
    static constexpr size_t BaseSize = BaseSize_;

    /** Constructor: initializes a new empty scratch buffer.
     *
     *  @returns no return value.
     */
    ScratchBuffer()
    {
        m_dynamicArray = m_localArray;
        m_size = BaseSize;
        m_capacity = BaseSize;
    }

    /** Copy constructor: copies a scratch buffer from another one.
     *
     *  @param[in] right    The other scratch buffer to copy.  The contents of the buffer will
     *                      be copied into this object and this object will be resized to match
     *                      the size of @p right.  The other object will not be modified.
     *  @returns no return value.
     */
    ScratchBuffer(const ScratchBuffer& right)
    {
        *this = right;
    }

    /** Move constructor: moves another scratch buffer into this one
     *
     *  @param[in] right    The other scratch buffer to move from.  The contents of the buffer
     *                      will be copied into this object and this object will be resized to match
     *                      the size of @p right.  The other object will be reset back to an empty
     *                      state after the data is moved.
     *  @returns no return value.
     */
    ScratchBuffer(ScratchBuffer&& right)
    {
        *this = std::move(right);
    }

    ~ScratchBuffer()
    {
        if (m_dynamicArray != nullptr && m_dynamicArray != m_localArray)
            delete[] m_dynamicArray;
    }

    /** Assignment operator (copy).
     *
     *  @param[in] right    The other scratch buffer to copy from.
     *  @returns A reference to this object.  All items in the @p right buffer will be copied
     *           into this object by the most efficient means possible.
     */
    ScratchBuffer& operator=(const ScratchBuffer& right)
    {
        if (&right == this)
            return *this;

        if (!resize(right.m_size))
            return *this;

        std::copy_n(right.m_dynamicArray, m_size, m_dynamicArray);

        return *this;
    }

    /** Assignment operator (move).
     *
     *  @param[in] right    The other scratch buffer to move from.
     *  @returns A reference to this object.  All items in the @p right buffer will be moved
     *           into this object by the most efficient means possible.  The @p right buffer
     *           will be cleared out upon return (this only really has a meaning if the
     *           contained type is a movable class).
     */
    ScratchBuffer& operator=(ScratchBuffer&& right)
    {
        if (&right == this)
            return *this;

        if (!resize(right.m_size))
            return *this;

        std::copy_n(std::make_move_iterator(right.m_dynamicArray), m_size, m_dynamicArray);

        return *this;
    }

    /** Array access operator.
     *
     *  @param[in] index    The index to access the array at.  It is the caller's responsibility
     *                      to ensure this index is within the bounds of the array.
     *  @returns A reference to the requested entry in the array as an lvalue.
     */
    T& operator[](size_t index)
    {
        return m_dynamicArray[index];
    }

    /** Array accessor.
     *
     *  @returns The base address of the buffer.  This address will be valid until the next
     *           call to resize().  It is the caller's responsibility to protect access to
     *           this buffer as needed.
     */
    T* data()
    {
        return m_dynamicArray;
    }

    /** @copydoc data() */
    const T* data() const
    {
        return m_dynamicArray;
    }

    /** Retrieves the current size of the buffer in items.
     *
     *  @returns The current size of this buffer in items.  This value is valid until the next
     *           call to resize().  The size in bytes can be calculated by multiplying this
     *           value by `sizeof(DataType)`.
     */
    size_t size() const
    {
        return m_size;
    }

    /** Attempts to resize this buffer.
     *
     *  @param[in] count    The new requested size of the buffer in items.  This may not be zero.
     *  @returns `true` if the buffer was successfully resized.  If the requested count is
     *           smaller than or equal to the @p BaseSize value for this object, resizing will
     *           always succeed.
     *  @returns `false` if the buffer could not be resized.  On failure, the previous contents
     *           and size of the array will be left unchanged.
     */
    bool resize(size_t count)
    {
        T* tmp = m_localArray;
        size_t copyCount;

        if (count == 0)
            return true;

        // the buffer is already the requested size -> nothing to do => succeed.
        if (count == m_size)
            return true;

        // the buffer is already large enough => don't resize unless the change is drastic.
        if (count <= m_capacity)
        {
            if ((count * 100) >= ShrinkThreshold_ * m_size)
            {
                m_size = count;
                return true;
            }
        }

        if (count > BaseSize)
        {
            tmp = new (std::nothrow) T[count];

            if (tmp == nullptr)
                return false;
        }

        // the buffer didn't change -> nothing to copy => update parameters and succeed.
        if (tmp == m_dynamicArray)
        {
            m_size = count;
            m_capacity = BaseSize;
            return true;
        }

        copyCount = m_size;

        if (m_size > count)
            copyCount = count;

        std::copy_n(std::make_move_iterator(m_dynamicArray), copyCount, tmp);

        if (m_dynamicArray != nullptr && m_dynamicArray != m_localArray)
            delete[] m_dynamicArray;

        m_dynamicArray = tmp;
        m_size = count;
        m_capacity = count;
        return true;
    }

private:
    /** The stack allocated space for the buffer.  This is always sized to be the larger of
     *  512 bytes or enough space to hold 16 items.  This array should never be accessed
     *  directly.  If this buffer is in use, the @p m_dynamicArray member will point to
     *  this buffer.
     */
    T m_localArray[BaseSize];

    /** The dynamically allocated buffer if the current size is larger than the base size
     *  of this buffer, or a pointer to @p m_localArray if smaller or equal.  This will
     *  never be `nullptr`.
     */
    T* m_dynamicArray;

    /** The current size of the buffer in items. */
    size_t m_size = BaseSize;

    /** The current maximum size of the buffer in items. */
    size_t m_capacity = BaseSize;
};

} // namespace extras
} // namespace omni
