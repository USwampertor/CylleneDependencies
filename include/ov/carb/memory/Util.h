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
//! @brief Helper utilities for memory
#pragma once

#include "../Defines.h"

#if CARB_PLATFORM_LINUX
#    include <unistd.h>
#endif

namespace carb
{
namespace memory
{

// Turn off optimization for testReadable() for Visual Studio, otherwise the read will be elided and it will always
// return true.
CARB_OPTIMIZE_OFF_MSC()

/**
 * Tests if a memory word (size_t) can be read from an address without crashing.
 *
 * @note This is not a particularly efficient function and should not be depended on for performance.
 *
 * @param mem The address to attempt to read
 * @returns `true` if a value could be read successfully, `false` if attempting to read the value would cause an access
 * violation or SIGSEGV.
 */
CARB_ATTRIBUTE(no_sanitize_address) inline bool testReadable(const void* mem)
{
#if CARB_PLATFORM_WINDOWS
    // Use SEH to catch a read failure. This is very fast unless an exception occurs as no setup work is needed on
    // x86_64.
    __try
    {
        size_t s = *reinterpret_cast<const size_t*>(mem);
        CARB_UNUSED(s);
        return true;
    }
    __except (1)
    {
        return false;
    }
#elif CARB_POSIX
    // The pipes trick: use the kernel to validate that the memory can be read. write() will return -1 with errno=EFAULT
    // if the memory is not readable.
    int pipes[2];
    CARB_FATAL_UNLESS(pipe(pipes) == 0, "Failed to create pipes");
    int ret = CARB_RETRY_EINTR(write(pipes[1], mem, sizeof(size_t)));
    CARB_FATAL_UNLESS(
        ret == sizeof(size_t) || errno == EFAULT, "Unexpected result from write(): {%d/%s}", errno, strerror(errno));
    close(pipes[0]);
    close(pipes[1]);
    return ret == sizeof(size_t);
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}
CARB_OPTIMIZE_ON_MSC()

/**
 * Copies memory as via memmove, but returns false if a read access violation occurs while reading.
 *
 * @rst
 * .. warning:: This function is designed for protection, not performance, and may be very slow to execute.
 * @endrst
 * @thread_safety This function is safe to call concurrently. However, this function makes no guarantees about the
 *     consistency of data copied when the data is modified while copied, only the attempting to read invalid memory
 *     will not result in an access violation.
 *
 * As with `memmove()`, the memory areas may overlap: copying takes place as though the bytes in @p source are first
 * copied into a temporary array that does not overlap @p source or @p dest, and the bytes are then copied from the
 * temporary array to @p dest.
 *
 * @param dest The destination buffer that will receive the copied bytes.
 * @param source The source buffer to copy bytes from.
 * @param len The number of bytes of @p source to copy to @p dest.
 * @returns `true` if the memory was successfully copied. If `false` is returned, @p dest is in a valid but undefined
 * state.
 */
CARB_ATTRIBUTE(no_sanitize_address) inline bool protectedMemmove(void* dest, const void* source, size_t len)
{
    if (!source)
        return false;

#if CARB_PLATFORM_WINDOWS
    // Use SEH to catch a read failure. This is very fast unless an exception occurs as no setup work is needed on
    // x86_64.
    __try
    {
        memmove(dest, source, len);
        return true;
    }
    __except (1)
    {
        return false;
    }
#elif CARB_POSIX
    // Create a pipe and read the data through the pipe. The kernel will sanitize the reads.
    int pipes[2];
    if (pipe(pipes) != 0)
        return false;

    while (len != 0)
    {
        ssize_t s = ::carb_min((ssize_t)len, (ssize_t)4096);
        if (CARB_RETRY_EINTR(write(pipes[1], source, s)) != s || CARB_RETRY_EINTR(read(pipes[0], dest, s)) != s)
            break;
        len -= size_t(s);
        dest = static_cast<uint8_t*>(dest) + s;
        source = static_cast<const uint8_t*>(source) + s;
    }

    close(pipes[0]);
    close(pipes[1]);
    return len == 0;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Copies memory as via strncpy, but returns false if an access violation occurs while reading.
 *
 * @rst
 * .. warning:: This function is designed for safety, not performance, and may be very slow to execute.
 * .. warning:: The `source` and `dest` buffers may not overlap.
 * @endrst
 * @thread_safety This function is safe to call concurrently. However, this function makes no guarantees about the
 *     consistency of data copied when the data is modified while copied, only the attempting to read invalid memory
 *     will not result in an access violation.
 * @param dest The destination buffer that will receive the memory. Must be at least @p n bytes in size.
 * @param source The source buffer. Up to @p n bytes will be copied.
 * @param n The maximum number of characters of @p source to copy to @p dest. If no `NUL` character was encountered in
 *     the first `n - 1` characters of `source`, then `dest[n - 1]` will be a `NUL` character. This is a departure
 *     from `strncpy()` but similar to `strncpy_s()`.
 * @returns `true` if the memory was successfully copied; `false` otherwise. If `false` is returned, `dest` is in a
 *     valid but undefined state.
 */
CARB_ATTRIBUTE(no_sanitize_address) inline bool protectedStrncpy(char* dest, const char* source, size_t n)
{
    if (!source)
        return false;

#if CARB_PLATFORM_WINDOWS
    // Use SEH to catch a read failure. This is very fast unless an exception occurs as no setup work is needed on
    // x86_64.
    __try
    {
        size_t len = strnlen(source, n - 1);
        memcpy(dest, source, len);
        dest[len] = '\0';
        return true;
    }
    __except (1)
    {
        return false;
    }
#elif CARB_POSIX
    if (n == 0)
        return false;

    // Create a pipe and read the data through the pipe. The kernel will sanitize the reads.
    struct Pipes
    {
        bool valid;
        int fds[2];
        Pipes()
        {
            valid = pipe(fds) == 0;
        }
        ~Pipes()
        {
            if (valid)
            {
                close(fds[0]);
                close(fds[1]);
            }
        }
        int operator[](int p) const noexcept
        {
            return fds[p];
        }
    } pipes;

    if (!pipes.valid)
        return false;

    constexpr static size_t kBytes = sizeof(size_t);
    constexpr static size_t kMask = kBytes - 1;

    // Unaligned reads
    while (n != 0 && (size_t(source) & kMask) != 0)
    {
        if (CARB_RETRY_EINTR(write(pipes[1], source, 1)) != 1 || CARB_RETRY_EINTR(read(pipes[0], dest, 1)) != 1)
            return false;
        if (*dest == '\0')
            return true;
        ++source, ++dest, --n;
    }

    // Aligned reads
    while (n >= kBytes)
    {
        CARB_ASSERT((size_t(source) & kMask) == 0);
        union
        {
            size_t value;
            char chars[kBytes];
        } u;
        if (CARB_RETRY_EINTR(write(pipes[1], source, kBytes)) != kBytes ||
            CARB_RETRY_EINTR(read(pipes[0], &u.value, kBytes)) != kBytes)
            return false;
        // Use the strlen bit trick to check if any bytes that make up a word are definitely not zero
        if (CARB_UNLIKELY(((u.value - 0x0101010101010101) & 0x8080808080808080)))
        {
            // One of the bytes could be zero
            for (int i = 0; i != sizeof(size_t); ++i)
            {
                dest[i] = u.chars[i];
                if (!dest[i])
                    return true;
            }
        }
        else
        {
            memcpy(dest, u.chars, kBytes);
        }
        source += kBytes;
        dest += kBytes;
        n -= kBytes;
    }

    // Trailing reads
    while (n != 0)
    {
        if (CARB_RETRY_EINTR(write(pipes[1], source, 1)) != 1 || CARB_RETRY_EINTR(read(pipes[0], dest, 1)) != 1)
            return false;
        if (*dest == '\0')
            return true;
        ++source, ++dest, --n;
    }

    // Truncate
    *(dest - 1) = '\0';
    return true;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

} // namespace memory
} // namespace carb
