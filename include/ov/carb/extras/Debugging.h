// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides some helper functions that check the state of debuggers for the calling
 *         process.
 */
#pragma once

#include "../Defines.h"

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
extern "C"
{
    // Forge doesn't define these functions, and it can be included before CarbWindows.h in some cases
    // So ensure that they're defined here.
    __declspec(dllimport) BOOL __stdcall IsDebuggerPresent(void);
    __declspec(dllimport) void __stdcall DebugBreak(void);

    // Undocumented function from ntdll.dll, only present in ntifs.h from the Driver Development Kit
    __declspec(dllimport) unsigned short __stdcall RtlCaptureStackBackTrace(unsigned long,
                                                                            unsigned long,
                                                                            void**,
                                                                            unsigned long*);
}

// Some things define vsnprintf (pyerrors.h, for example)
#    ifdef vsnprintf
#        undef vsnprintf
#    endif
#elif CARB_POSIX
#    include <chrono>
#    include <execinfo.h>
#    include <signal.h>
#    include <stdint.h>
#    include <string.h>
#    if CARB_PLATFORM_MACOS
#        include <sys/sysctl.h>
#        include <sys/types.h>
#        include <unistd.h>
#        include <mach/mach.h>
#        include <mach/vm_map.h>
#        include <pthread/stack_np.h>
#    endif
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

#include <cstdio>

/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Common namespace for extra helper functions and classes. */
namespace extras
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#    if CARB_PLATFORM_MACOS
namespace detail
{

inline bool getVMInfo(uint8_t const* const addr, uint8_t** top, uint8_t** bot) noexcept
{
    vm_address_t address = (vm_address_t)addr;
    vm_size_t size = 0;
    vm_region_basic_info_data_64_t region{};
    mach_msg_type_number_t regionSize = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj{};
    kern_return_t ret = vm_region_64(
        mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&region, &regionSize, &obj);
    if (ret != KERN_SUCCESS)
        return false;
    *bot = (uint8_t*)address;
    *top = *bot + size;
    return true;
}

#        define INSTACK(a) ((a) >= stackbot && (a) <= stacktop)
#        if defined(__x86_64__)
#            define ISALIGNED(a) ((((uintptr_t)(a)) & 0xf) == 0)
#        elif defined(__i386__)
#            define ISALIGNED(a) ((((uintptr_t)(a)) & 0xf) == 8)
#        elif defined(__arm__) || defined(__arm64__)
#            define ISALIGNED(a) ((((uintptr_t)(a)) & 0x1) == 0)
#        endif

// Use the ol' static-functions-in-a-template-class to prevent the linker complaining about duplicated symbols.
template <class T = void>
struct Utils
{

    __attribute__((noinline)) static void internalBacktrace(
        vm_address_t* buffer, size_t maxCount, size_t* nb, size_t skip, void* startfp) noexcept
    {
        uint8_t *frame, *next;
        pthread_t self = pthread_self();
        uint8_t* stacktop = static_cast<uint8_t*>(pthread_get_stackaddr_np(self));
        uint8_t* stackbot = stacktop - pthread_get_stacksize_np(self);

        *nb = 0;

        // Rely on the fact that our caller has an empty stackframe (no local vars)
        // to determine the minimum size of a stackframe (frame ptr & return addr)
        frame = static_cast<uint8_t*>(__builtin_frame_address(0));
        next = reinterpret_cast<uint8_t*>(pthread_stack_frame_decode_np((uintptr_t)frame, nullptr));

        /* make sure return address is never out of bounds */
        stacktop -= ((uintptr_t)next - (uintptr_t)frame);

        if (!INSTACK(frame) || !ISALIGNED(frame))
        {
            // Possibly running as a fiber, get the region info for the memory in question
            if (!getVMInfo(frame, &stacktop, &stackbot))
                return;
            if (!INSTACK(frame) || !ISALIGNED(frame))
                return;
        }
        while (startfp || skip--)
        {
            if (startfp && startfp < next)
                break;
            if (!INSTACK(next) || !ISALIGNED(next) || next <= frame)
                return;
            frame = next;
            next = reinterpret_cast<uint8_t*>(pthread_stack_frame_decode_np((uintptr_t)frame, nullptr));
        }
        while (maxCount--)
        {
            uintptr_t retaddr;
            next = reinterpret_cast<uint8_t*>(pthread_stack_frame_decode_np((uintptr_t)frame, &retaddr));
            buffer[*nb] = retaddr;
            (*nb)++;
            if (!INSTACK(next) || !ISALIGNED(next) || next <= frame)
                return;
            frame = next;
        }
    }

    __attribute__((disable_tail_calls)) static void backtrace(
        vm_address_t* buffer, size_t maxCount, size_t* nb, size_t skip, void* startfp) noexcept
    {
        // callee relies upon no tailcall and no local variables
        internalBacktrace(buffer, maxCount, nb, skip + 1, startfp);
    }
};

#        undef INSTACK
#        undef ISALIGNED
} // namespace detail
#    endif
#endif

/**
 *  Checks if a debugger is attached to the calling process.
 *
 *  @returns `true` if a user-mode debugger is currently attached to the calling process.
 *  @returns `false` if the calling process does not have a debugger attached.
 *
 *  @remarks This checks if a debugger is currently attached to the calling process.  This will
 *           query the current state of the debugger so that a process can disable some debug-
 *           only checks at runtime if the debugger is ever detached.  If a debugger is attached
 *           to a running process, this will start returning `true` again.
 *
 *  @note    On Linux and related platforms, the debugger check is a relatively expensive
 *           operation.  To avoid unnecessary overhead explicitly checking this state each time,
 *           the last successfully queried state will be cached for a short period.  Future calls
 *           within this period will simply return the cached state instead of querying again.
 */
inline bool isDebuggerAttached(void)
{
#if CARB_PLATFORM_WINDOWS
    return IsDebuggerPresent();
#elif CARB_PLATFORM_LINUX
    // the maximum amount of time in milliseconds that the cached debugger state is valid for.
    // If multiple calls to isDebuggerAttached() are made within this period, a cached state
    // will be returned instead of re-querying.  Outside of this period however, a new call
    // to check the debugger state with isDebuggerAttached() will cause the state to be
    // refreshed.
    static constexpr uint64_t kDebugUtilsDebuggerCheckPeriod = 500;
    static bool queried = false;
    static bool state = false;
    static std::chrono::high_resolution_clock::time_point lastCheckTime = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point t = std::chrono::high_resolution_clock::now();
    uint64_t millisecondsElapsed =
        std::chrono::duration_cast<std::chrono::duration<uint64_t, std::milli>>(t - lastCheckTime).count();


    if (!queried || millisecondsElapsed > kDebugUtilsDebuggerCheckPeriod)
    {
        // on Android and Linux we can check the '/proc/self/status' file for the line
        // "TracerPid:" to check if the associated value is *not* 0.  Note that this is
        // not a cheap check so we'll cache its result and only re-query once every few
        // seconds.
        FILE* fp;
        char line[256];
        char* tok;
        char* endp;


        fp = fopen("/proc/self/status", "r");

        // assume no debugger attached.
        state = false;

        // failed to open the status file (!?) -> nothing to detect => fail.
        if (fp == nullptr)
            return false;

        while (1)
        {
            tok = fgets(line, CARB_COUNTOF32(line), fp);

            if (tok == nullptr || feof(fp) || ferror(fp))
                break;

            tok = strtok_r(line, " \t\n\r", &endp);

            if (tok == nullptr)
                continue;

            if (strcasecmp(tok, "TracerPid:") == 0)
            {
                tok = strtok_r(nullptr, " \t\n\r", &endp);

                if (tok != nullptr && atoi(tok) != 0)
                    state = true;

                break;
            }
        }

        fclose(fp);
        lastCheckTime = t;
        queried = true;
    }

    return state;
#elif CARB_PLATFORM_MACOS
    int mib[] = {
        CTL_KERN,
        KERN_PROC,
        KERN_PROC_PID,
        getpid(),
    };
    struct kinfo_proc info = {};
    size_t size = sizeof(info);

    // Ignore the return value. It'll return false if this fails.
    sysctl(mib, CARB_COUNTOF(mib), &info, &size, nullptr, 0);

    return (info.kp_proc.p_flag & P_TRACED) != 0;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 *  Performs a software breakpoint if a debugger is currently attached to this process.
 *
 *  @returns No return value.
 *
 *  @remarks This performs a software breakpoint.  If a debugger is attached, this will cause the
 *           breakpoint to be caught and the debugger will take over the process' state.  If no
 *           debugger is attached, this will be ignored.  This can be thought of as a more dynamic
 *           version of CARB_ASSERT(0) where the existence of a debugger is explicitly checked
 *           at runtime instead of at compile time.
 *
 *  @note    This should really only be used for local debugging purposes.  The debugger check
 *           that is used in here (isDebuggerAttached()) could potentially be expensive on some
 *           platforms, so this should only be called when it is well known that a problem that
 *           needs immediate debugging attention has already occurred.
 */
inline void debuggerBreak(void)
{
    if (!isDebuggerAttached())
        return;

#if CARB_PLATFORM_WINDOWS
    DebugBreak();
#elif CARB_POSIX
    // NOTE: the __builtin_trap() call is the more 'correct and portable' way to do this.  However
    //       that unfortunately raises a SIGILL signal which is not continuable (at least not in
    //       MSVC Android or GDB) so its usefulness in a debugger is limited.  Directly raising a
    //       SIGTRAP signal instead still gives the desired behavior and is also continuable.
    raise(SIGTRAP);
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Attempts to capture the callstack for the current thread.
 *
 * @note Non-debug code and lack of symbols can cause this function to be unable to capture stack frames. Function
 * inlining and tail-calls may result in functions being absent from the backtrace.
 *
 * @param skipFrames The number of callstack frames to skip from the start (current call point) of the backtrace.
 * @param array The array of pointers that is populated with the backtrace. The caller allocates this array.
 * @param count The number of pointers that @p array can hold; the maximum size of the captured backtrace.
 *
 * @return The number of backtrace frames captured in @p array
 */
inline size_t debugBacktrace(size_t skipFrames, void** array, size_t count) noexcept
{
#if CARB_PLATFORM_WINDOWS
    // Apparently RtlCaptureStackBackTrace() can "fail" (i.e. not write anything and return 0) without setting any
    // error for GetLastError(). Try a few times in a loop.
    constexpr static int kRetries = 3;
    for (int i = 0; i != kRetries; ++i)
    {
        unsigned short frames =
            ::RtlCaptureStackBackTrace((unsigned long)skipFrames, (unsigned long)count, array, nullptr);
        if (frames)
            return frames;
    }
    // Failed
    return 0;
#elif CARB_PLATFORM_LINUX
    void** target = array;
    if (skipFrames)
    {
        target = CARB_STACK_ALLOC(void*, count + skipFrames);
    }
    size_t captured = (size_t)::backtrace(target, int(count + skipFrames));
    if (captured <= skipFrames)
        return 0;

    if (skipFrames)
    {
        captured -= skipFrames;
        memcpy(array, target + skipFrames, sizeof(void*) * captured);
    }
    return captured;
#elif CARB_PLATFORM_MACOS
    size_t num_frames;
    detail::Utils<>::backtrace(reinterpret_cast<vm_address_t*>(array), count, &num_frames, skipFrames, nullptr);
    while (num_frames >= 1 && array[num_frames - 1] == nullptr)
        num_frames -= 1;
    return num_frames;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Prints a formatted string to debug output. On Windows this means, OutputDebugString; on Linux, this just prints to
 * the console.
 *
 * @note This should never be used in production code!
 *
 * On Windows, the Sysinternals DebugView utility can be used to view the debug strings outside of a debugger.
 * @param fmt The printf-style format string
 */
void debugPrint(const char* fmt, ...) CARB_PRINTF_FUNCTION(1, 2);
inline void debugPrint(const char* fmt, ...)
{
#if CARB_PLATFORM_WINDOWS
    va_list va, va2;
    va_start(va, fmt);

    va_copy(va2, va);
    int count = vsnprintf(nullptr, 0, fmt, va2);
    va_end(va2);
    if (count > 0)
    {
        char* buffer = CARB_STACK_ALLOC(char, size_t(count) + 1);
        vsnprintf(buffer, size_t(count + 1), fmt, va);
        ::OutputDebugStringA(buffer);
    }
    va_end(va);
#else
    va_list va;
    va_start(va, fmt);
    vfprintf(stdout, fmt, va);
    va_end(va);
#endif
}

} // namespace extras
} // namespace carb
