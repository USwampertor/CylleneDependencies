// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

/**
 * @file
 * @brief Allow access to specific parts of Windows API without including Windows.h
 *
 * This file replaces `#include <Windows.h>` for header files. *Windows.h* is monolithic and defines some oft-used
 * names like `min`, `max`, `count`, etc. Directives like `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, and `WIN32_EXTRA_LEAN` can
 * help, but still leave the global namespace much more polluted than is desired. Note that the MSVC CRT does not
 * include *Windows.h* anywhere; instead special reserved naming conventions (such as `_PrefixedUpper`) exist to allow
 * the CRT to define special functions that are called by the CRT, but the implementation of those functions
 * (that utilize the Windows API) are hidden away in static or dynamic libraries. However, for Carbonite, since the
 * goal is to not require additional linking for header utilities, there is no ability for `carb` header files to
 * similarly hide Windows API usage in libraries.
 *
 * Anything from the Windows API that Carbonite relies upon should be defined in this file, and should not conflict in
 * the event that *Windows.h* is included.
 *
 * Rules for adding things to this file:
 * 1. Do not `#define` anything that is defined in *Windows.h*. Instead, prefix the macro name with `CARBWIN_` but give
 *    it the same value. This is to prevent errors if *Windows.h* is included since macros may not be defined more than
 *    once.
 * 2. Typedef's should be specified _exactly_ as in Windows.h
 * 3. Structs can be forward-declared only, otherwise definitions will conflict with *Windows.h*. If a struct
 *    definition is required, it should be prefixed with `CARBWIN_` at the bottom of this file. *TestCarbWindows.cpp*
 *    should then `static_assert` that the size and member offsets are the same as the *Windows.h* version.
 * 4. Enums cannot be redefined. Therefore, enum values needed should be changed to `#define` and prefixed with
 *    `CARBWIN_`.
 *
 * These rules will allow compilation units to `#include <Windows.h>` before or after this file.
 */
#pragma once

#include "Defines.h"

CARB_IGNOREWARNING_MSC_WITH_PUSH(4201) // nonstandard extension used: nameless struct/union

// clang-format off
#if CARB_PLATFORM_WINDOWS && !defined(DOXYGEN_BUILD)

#ifndef __cplusplus
#define CARBWIN_NONAMELESSUNION // use strict ANSI standard
#else
extern "C"
{
#endif

// Define these temporarily so that they don't conflict with Windows.h. They are #undef'd at the bottom
#define CARBWIN_WINBASEAPI __declspec(dllimport)
#ifndef WINBASEAPI
#define WINBASEAPI CARBWIN_WINBASEAPI
#define CARBWIN_WINBASEAPI_DEFINED 1
#endif
#define CARBWIN_WINAPI __stdcall
#ifndef WINAPI
#define WINAPI CARBWIN_WINAPI
#define CARBWIN_WINAPI_DEFINED 1
#endif
#define CARBWIN_SHSTDAPI extern "C" __declspec(dllimport) HRESULT WINAPI
#define CARBWIN_SHSTDAPI_(type) extern "C" __declspec(dllimport) type WINAPI
#ifndef SHSTDAPI
#define SHSTDAPI CARBWIN_SHSTDAPI
#define SHSTDAPI_(type) CARBWIN_SHSTDAPI_(type)
#define CARBWIN_SHSTDAPI_DEFINED 1
#endif
#define CARBWIN_APIENTRY __stdcall
#ifndef APIENTRY
#define APIENTRY CARBWIN_APIENTRY
#define CARBWIN_APIENTRY_DEFINED 1
#endif
#define CARBWIN_WINADVAPI __declspec(dllimport)
#ifndef WINADVAPI
#define WINADVAPI CARBWIN_WINADVAPI
#define CARBWIN_WINADVAPI_DEFINED 1
#endif

///////////////////////////////////////////////////////////////////////////////
// #defines. Should be prefixed with CARBWIN_ and defined exactly the same as
// their Windows.h counterpart.

// from minwindef.h
#define CARBWIN_CONST const
#define CARBWIN_FALSE 0
#define CARBWIN_TRUE 1
#define CARBWIN_MAX_PATH 260

// from winnt.h
#ifndef CARBWIN_DUMMYUNIONNAME
#if defined(CARBWIN_NONAMELESSUNION) || !defined(_MSC_EXTENSIONS)
#define CARBWIN_DUMMYUNIONNAME u
#else
#define CARBWIN_DUMMYUNIONNAME
#endif
#endif
#define CARBWIN_STATUS_SUCCESS                   ((DWORD   )0x00000000L)
#define CARBWIN_STATUS_TIMEOUT                   ((DWORD   )0x00000102L)
#define CARBWIN_MEM_COMMIT              0x00001000
#define CARBWIN_MEM_RESERVE             0x00002000
#define CARBWIN_MEM_DECOMMIT            0x00004000
#define CARBWIN_MEM_RELEASE             0x00008000
#define CARBWIN_MEM_FREE                0x00010000
#define CARBWIN_MEM_PRIVATE             0x00020000
#define CARBWIN_MEM_MAPPED              0x00040000
#define CARBWIN_MEM_RESET               0x00080000
#define CARBWIN_MEM_TOP_DOWN            0x00100000
#define CARBWIN_MEM_WRITE_WATCH         0x00200000
#define CARBWIN_MEM_PHYSICAL            0x00400000
#define CARBWIN_MEM_LARGE_PAGES         0x20000000
#define CARBWIN_MEM_4MB_PAGES           0x80000000

#ifndef CARBWIN_DUMMYSTRUCTNAME
#if defined(CARBWIN_NONAMELESSUNION) || !defined(_MSC_EXTENSIONS)
#define CARBWIN_DUMMYSTRUCTNAME s
#else
#define CARBWIN_DUMMYSTRUCTNAME
#endif
#endif

#define CARBWIN_VOID void
#define CARBWIN_DLL_PROCESS_ATTACH   1
#define CARBWIN_DLL_THREAD_ATTACH    2
#define CARBWIN_DLL_THREAD_DETACH    3
#define CARBWIN_DLL_PROCESS_DETACH   0
#define CARBWIN_VOID void
#define CARBWIN_STATUS_WAIT_0  ((DWORD)0x00000000L)
#define CARBWIN_RTL_SRWLOCK_INIT {0}
#define CARBWIN_MAKELANGID(p, s)       ((((WORD  )(s)) << 10) | (WORD  )(p))
#define CARBWIN_MAKELCID(lgid, srtid)  ((DWORD)((((DWORD)((WORD)(srtid))) << 16) | ((DWORD)((WORD)(lgid)))))
#define CARBWIN_LANG_NEUTRAL                     0x00
#define CARBWIN_LANG_INVARIANT                   0x7f
#define CARBWIN_SUBLANG_DEFAULT                  0x01
#define CARBWIN_SUBLANG_NEUTRAL                  0x00
#define CARBWIN_SORT_DEFAULT                     0x0
#define CARBWIN_PAGE_READONLY           0x02
#define CARBWIN_PAGE_READWRITE          0x04
#define CARBWIN_STANDARD_RIGHTS_REQUIRED         (0x000F0000L)
#define CARBWIN_SECTION_QUERY                0x0001
#define CARBWIN_SECTION_MAP_WRITE            0x0002
#define CARBWIN_SECTION_MAP_READ             0x0004
#define CARBWIN_SECTION_MAP_EXECUTE          0x0008
#define CARBWIN_SECTION_EXTEND_SIZE          0x0010
#define CARBWIN_SECTION_MAP_EXECUTE_EXPLICIT 0x0020
#define CARBWIN_SECTION_ALL_ACCESS (CARBWIN_STANDARD_RIGHTS_REQUIRED|CARBWIN_SECTION_QUERY|\
                            CARBWIN_SECTION_MAP_WRITE |      \
                            CARBWIN_SECTION_MAP_READ |       \
                            CARBWIN_SECTION_MAP_EXECUTE |    \
                            CARBWIN_SECTION_EXTEND_SIZE)

#define CARBWIN_LOCALE_INVARIANT                                                      \
          (CARBWIN_MAKELCID(CARBWIN_MAKELANGID(CARBWIN_LANG_INVARIANT, CARBWIN_SUBLANG_NEUTRAL), CARBWIN_SORT_DEFAULT))

#define CARBWIN_LCMAP_LOWERCASE           0x00000100
#define CARBWIN_LCMAP_UPPERCASE           0x00000200
#define CARBWIN_LCMAP_TITLECASE           0x00000300
#define CARBWIN_LCMAP_SORTKEY             0x00000400
#define CARBWIN_LCMAP_BYTEREV             0x00000800
#define CARBWIN_LCMAP_HIRAGANA            0x00100000
#define CARBWIN_LCMAP_KATAKANA            0x00200000
#define CARBWIN_LCMAP_HALFWIDTH           0x00400000
#define CARBWIN_LCMAP_FULLWIDTH           0x00800000
#define CARBWIN_LCMAP_LINGUISTIC_CASING   0x01000000
#define CARBWIN_LCMAP_SIMPLIFIED_CHINESE  0x02000000
#define CARBWIN_LCMAP_TRADITIONAL_CHINESE 0x04000000
#define CARBWIN_LCMAP_SORTHANDLE          0x20000000
#define CARBWIN_LCMAP_HASH                0x00040000

#define CARBWIN_FILE_SHARE_READ           0x00000001
#define CARBWIN_FILE_SHARE_WRITE          0x00000002
#define CARBWIN_FILE_SHARE_DELETE         0x00000004

#define CARBWIN_GENERIC_READ              (0x80000000L)
#define CARBWIN_GENERIC_WRITE             (0x40000000L)
#define CARBWIN_GENERIC_EXECUTE           (0x20000000L)
#define CARBWIN_GENERIC_ALL               (0x10000000L)

#define CARBWIN_EXCEPTION_NONCONTINUABLE  0x1

#define CARBWIN_EVENTLOG_SEQUENTIAL_READ        0x0001
#define CARBWIN_EVENTLOG_SEEK_READ              0x0002
#define CARBWIN_EVENTLOG_FORWARDS_READ          0x0004
#define CARBWIN_EVENTLOG_BACKWARDS_READ         0x0008

#define CARBWIN_EVENTLOG_SUCCESS                0x0000
#define CARBWIN_EVENTLOG_ERROR_TYPE             0x0001
#define CARBWIN_EVENTLOG_WARNING_TYPE           0x0002
#define CARBWIN_EVENTLOG_INFORMATION_TYPE       0x0004
#define CARBWIN_EVENTLOG_AUDIT_SUCCESS          0x0008
#define CARBWIN_EVENTLOG_AUDIT_FAILURE          0x0010

#define CARBWIN_EVENTLOG_START_PAIRED_EVENT    0x0001
#define CARBWIN_EVENTLOG_END_PAIRED_EVENT      0x0002
#define CARBWIN_EVENTLOG_END_ALL_PAIRED_EVENTS 0x0004
#define CARBWIN_EVENTLOG_PAIRED_EVENT_ACTIVE   0x0008
#define CARBWIN_EVENTLOG_PAIRED_EVENT_INACTIVE 0x0010

// from intsafe.h
#define CARBWIN_S_OK ((HRESULT)0L)

// from handleapi.h
#define CARBWIN_INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

// from winbase.h
#define CARBWIN_WAIT_OBJECT_0 ((CARBWIN_STATUS_WAIT_0) + 0)
#define CARBWIN_INFINITE 0xFFFFFFFF
#define CARBWIN_FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define CARBWIN_FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define CARBWIN_FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define CARBWIN_FIBER_FLAG_FLOAT_SWITCH 0x1 // context switch floating point

#define CARBWIN_FILE_ATTRIBUTE_NORMAL          0x80
#define CARBWIN_FILE_FLAG_BACKUP_SEMANTICS     0x02000000

// from winerror.h
#define CARBWIN_ERROR_SUCCESS                    0L
#define CARBWIN_ERROR_PATH_NOT_FOUND             3L
#define CARBWIN_ERROR_ACCESS_DENIED              5L
#define CARBWIN_ERROR_INSUFFICIENT_BUFFER        122L
#define CARBWIN_ERROR_MOD_NOT_FOUND              126L
#define CARBWIN_ERROR_ALREADY_EXISTS             183L
#define CARBWIN_ERROR_FILENAME_EXCED_RANGE       206L
#define CARBWIN_WAIT_TIMEOUT                     258L
#define CARBWIN_ERROR_TIMEOUT                    1460L
#define CARBWIN_SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)

// from synchapi.h
#define CARBWIN_SRWLOCK_INIT CARBWIN_RTL_SRWLOCK_INIT

// from memoryapi.h
#define CARBWIN_FILE_MAP_READ CARBWIN_SECTION_MAP_READ
#define CARBWIN_FILE_MAP_ALL_ACCESS CARBWIN_SECTION_ALL_ACCESS

// from fileapi.h
#define CARBWIN_CREATE_NEW          1
#define CARBWIN_CREATE_ALWAYS       2
#define CARBWIN_OPEN_EXISTING       3
#define CARBWIN_OPEN_ALWAYS         4
#define CARBWIN_TRUNCATE_EXISTING   5
#define CARBWIN_INVALID_FILE_ATTRIBUTES ((DWORD)-1)

// from minwinbase.h
#define CARBWIN_LOCKFILE_FAIL_IMMEDIATELY   0x00000001
#define CARBWIN_LOCKFILE_EXCLUSIVE_LOCK     0x00000002

// from processthreadsapi.h
#define CARBWIN_TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)

// from pathcch.h
#define CARBWIN_PATHCCH_ALLOW_LONG_PATHS 0x01

// from excpt.h
#define CARBWIN_EXCEPTION_EXECUTE_HANDLER      1

///////////////////////////////////////////////////////////////////////////////
// typedefs, forward-declarations

// Many of the typedefs below are not compatible with Forge's own version of the Windows typedefs.
// These are more correct as they're declared exactly the same as in the Windows headers.
#ifndef NV_FORGE_WINDEF_H
// from basetsd.h
static_assert(sizeof(void*) == 8, "This only supports 64-bit platforms");
typedef unsigned int UINT32, *PUINT32;
typedef __int64 INT_PTR, *PINT_PTR;
typedef __int64 LONG_PTR, *PLONG_PTR;
typedef unsigned __int64 UINT_PTR, *PUINT_PTR;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR SIZE_T, *PSIZE_T;
typedef LONG_PTR SSIZE_T, *PSSIZE_T;
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;
typedef unsigned __int64 DWORD64, *PDWORD64;

// from WTypesbase.h
typedef wchar_t WCHAR;
typedef const WCHAR *LPCWSTR;

// from minwindef.h
typedef int BOOL;
typedef unsigned char BYTE;
typedef long *LPLONG;
typedef unsigned long DWORD;
typedef DWORD *LPDWORD;
typedef unsigned long ULONG, *PULONG;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef void *HANDLE;
typedef HANDLE HLOCAL;
typedef struct HINSTANCE__ *HINSTANCE;
typedef HINSTANCE HMODULE;
typedef INT_PTR (WINAPI *FARPROC)();
typedef struct _FILETIME FILETIME, *PFILETIME, *LPFILETIME;

typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

    // from minwinbase.h
typedef struct _SECURITY_ATTRIBUTES _SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
typedef DWORD *PDWORD;

// from winnt.h
typedef void *PVOID;
typedef long LONG;
typedef long HRESULT;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef BYTE BOOLEAN;
typedef unsigned short WORD;
typedef CHAR *LPSTR;
typedef const CHAR *LPCSTR, *PCSTR;
typedef WCHAR *NWPSTR, *LPWSTR, *PWSTR;
typedef const WCHAR *LPCWSTR, *PCWSTR;
typedef WCHAR *PWCHAR, *LPWCH, *PWCH;
typedef __int64 LONGLONG;
typedef unsigned __int64 ULONGLONG;
typedef ULONGLONG DWORDLONG;
typedef struct _RTL_SRWLOCK RTL_SRWLOCK, *PRTL_SRWLOCK;
typedef DWORD LCID;
typedef PDWORD PLCID;
typedef WORD LANGID;
typedef union _LARGE_INTEGER LARGE_INTEGER;
typedef LARGE_INTEGER *PLARGE_INTEGER;
typedef struct _EVENTLOGRECORD EVENTLOGRECORD, *PEVENTLOGRECORD;

///////////////////////////////////////////////////////////////////////////////
// Struct redefines
// See instructions for adding at the bottom of this block.

struct CARBWIN_SRWLOCK
{
    PVOID Ptr;
};

struct CARBWIN_PROCESS_MEMORY_COUNTERS {
    DWORD cb;
    DWORD PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
};

struct CARBWIN_MEMORYSTATUSEX {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORDLONG ullTotalPhys;
    DWORDLONG ullAvailPhys;
    DWORDLONG ullTotalPageFile;
    DWORDLONG ullAvailPageFile;
    DWORDLONG ullTotalVirtual;
    DWORDLONG ullAvailVirtual;
    DWORDLONG ullAvailExtendedVirtual;
};

struct CARBWIN_SYSTEM_INFO {
    union {
        DWORD dwOemId;          // Obsolete field...do not use
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        } CARBWIN_DUMMYSTRUCTNAME;
    } CARBWIN_DUMMYUNIONNAME;
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
};

struct CARBWIN_OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        } CARBWIN_DUMMYSTRUCTNAME;
        PVOID Pointer;
    } CARBWIN_DUMMYUNIONNAME;

    HANDLE  hEvent;
};

struct _OVERLAPPED;
typedef struct _OVERLAPPED* LPOVERLAPPED;

struct CARBWIN_FILE_NOTIFY_INFORMATION {
    DWORD NextEntryOffset;
    DWORD Action;
    DWORD FileNameLength;
    WCHAR FileName[1];
};

struct CARBWIN_FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

typedef union CARBWIN_LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    } CARBWIN_DUMMYSTRUCTNAME;
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} CARBWIN_LARGE_INTEGER;

struct CARBWIN_EVENTLOGRECORD {
    DWORD  Length;        // Length of full record
    DWORD  Reserved;      // Used by the service
    DWORD  RecordNumber;  // Absolute record number
    DWORD  TimeGenerated; // Seconds since 1-1-1970
    DWORD  TimeWritten;   // Seconds since 1-1-1970
    DWORD  EventID;
    WORD   EventType;
    WORD   NumStrings;
    WORD   EventCategory;
    WORD   ReservedFlags; // For use with paired events (auditing)
    DWORD  ClosingRecordNumber; // For use with paired events (auditing)
    DWORD  StringOffset;  // Offset from beginning of record
    DWORD  UserSidLength;
    DWORD  UserSidOffset;
    DWORD  DataLength;
    DWORD  DataOffset;    // Offset from beginning of record
    //
    // Then follow:
    //
    // WCHAR SourceName[]
    // WCHAR Computername[]
    // SID   UserSid
    // WCHAR Strings[]
    // BYTE  Data[]
    // CHAR  Pad[]
    // DWORD Length;
    //
};
 
struct CARBWIN_PROCESSOR_NUMBER {
    WORD Group;
    BYTE Number;
    BYTE Reserved;
};

typedef ULONG_PTR CARBWIN_KAFFINITY;
struct CARBWIN_GROUP_AFFINITY {
    CARBWIN_KAFFINITY Mask;
    WORD   Group;
    WORD   Reserved[3];
};

// ADD NEW STRUCT REDEFINES HERE
// - add to TestCarbWindows.cpp
// - add forward-declared typedefs exactly for Windows types above
// - Must be prefixed with CARBWIN_ and defined exactly as in Windows.h
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// functions

// from winbase.h
typedef void (WINAPI *PFIBER_START_ROUTINE)(LPVOID lpFiberParameter);
typedef PFIBER_START_ROUTINE LPFIBER_START_ROUTINE;
WINBASEAPI HLOCAL WINAPI LocalFree(HLOCAL hMem);
WINBASEAPI DWORD WINAPI FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, va_list *Arguments);
WINBASEAPI int WINAPI lstrlenW(LPCWSTR lpString);
WINBASEAPI LPVOID WINAPI CreateFiber(SIZE_T dwStackSize, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter);
WINBASEAPI LPVOID WINAPI CreateFiberEx(SIZE_T dwStackCommitSize, SIZE_T dwStackReserveSize, DWORD dwFlags, LPFIBER_START_ROUTINE lpStartAddress, LPVOID lpParameter);
WINBASEAPI void WINAPI DeleteFiber(LPVOID lpFiber);
WINBASEAPI void WINAPI SwitchToFiber(LPVOID lpFiber);
WINBASEAPI LPVOID WINAPI ConvertThreadToFiber(LPVOID lpParameter);
WINBASEAPI LPVOID WINAPI ConvertThreadToFiberEx(LPVOID lpParameter, DWORD dwFlags);
WINBASEAPI BOOL WINAPI ConvertFiberToThread();
WINBASEAPI DWORD_PTR WINAPI SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask);
WINADVAPI HANDLE WINAPI OpenEventLogW(LPCWSTR lpUNCServerName, LPCWSTR lpSourceName);
WINADVAPI BOOL WINAPI CloseEventLog(HANDLE hEventLog);
WINADVAPI BOOL WINAPI ReadEventLogW(HANDLE hEventLog, DWORD dwReadFlags, DWORD dwRecordOffset, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, DWORD *pnBytesRead, DWORD *pnMinNumberOfBytesNeeded);

// from debugapi.h
WINBASEAPI BOOL WINAPI IsDebuggerPresent(void);
WINBASEAPI void WINAPI DebugBreak(void);
WINBASEAPI void WINAPI OutputDebugStringA(LPCSTR lpOutputString);

// from synchapi.h
typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;
WINBASEAPI HANDLE WINAPI CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName);
WINBASEAPI BOOL WINAPI ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount);
WINBASEAPI DWORD WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
WINBASEAPI void WINAPI InitializeSRWLock(PSRWLOCK SRWLock);
WINBASEAPI void WINAPI ReleaseSRWLockExclusive(PSRWLOCK SRWLock);
WINBASEAPI void WINAPI ReleaseSRWLockShared(PSRWLOCK SRWLock);
WINBASEAPI void WINAPI AcquireSRWLockExclusive(PSRWLOCK SRWLock);
WINBASEAPI void WINAPI AcquireSRWLockShared(PSRWLOCK SRWLock);
WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockExclusive(PSRWLOCK SRWLock);
WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockShared(PSRWLOCK SRWLock);
WINBASEAPI HANDLE WINAPI CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOnwer, LPCSTR lpName);
WINBASEAPI HANDLE WINAPI CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName);
WINBASEAPI BOOL WINAPI ReleaseMutex(HANDLE hMutex);
WINBASEAPI HANDLE WINAPI CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
WINBASEAPI HANDLE WINAPI CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
WINBASEAPI void WINAPI Sleep(DWORD dwMilliseconds);
WINBASEAPI DWORD WINAPI SleepEx(DWORD dwMilliseconds, BOOL bAlertable);
// Note that these functions are not in DLLs; you must link with synchronization.lib
BOOL WINAPI WaitOnAddress(volatile void* Address, PVOID CompareAddress, SIZE_T AddressSize, DWORD dwMilliseconds);
void WINAPI WakeByAddressSingle(PVOID Address);
void WINAPI WakeByAddressAll(PVOID Address);

// from shellapi.h
SHSTDAPI_(LPWSTR *) CommandLineToArgvW(LPCWSTR lpCmdLine, int* pNumArgs);

// from processenv.h
WINBASEAPI LPWSTR WINAPI GetCommandLineW(void);
WINBASEAPI LPWCH WINAPI GetEnvironmentStringsW(void);
WINBASEAPI BOOL WINAPI FreeEnvironmentStringsW(LPWCH penv);
WINBASEAPI BOOL WINAPI SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);
WINBASEAPI DWORD WINAPI GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);

// from handleapi.h
WINBASEAPI BOOL WINAPI CloseHandle(HANDLE hObject);

// from errhandlingapi.h
WINBASEAPI void WINAPI RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR*);
WINBASEAPI DWORD WINAPI GetLastError(void);
WINBASEAPI void WINAPI SetLastError(DWORD dwErrCode);

// from processthreadsapi.h
typedef struct _PROCESSOR_NUMBER PROCESSOR_NUMBER, *PPROCESSOR_NUMBER;
WINBASEAPI HANDLE WINAPI GetCurrentProcess(void);
WINBASEAPI DWORD WINAPI GetCurrentProcessId(void);
WINBASEAPI HANDLE WINAPI GetCurrentThread(void);
WINBASEAPI DWORD WINAPI GetCurrentThreadId(void);
WINBASEAPI DWORD WINAPI GetThreadId(HANDLE);
WINBASEAPI BOOL WINAPI TerminateProcess(HANDLE, UINT);
WINBASEAPI DWORD WINAPI TlsAlloc(void);
WINBASEAPI LPVOID WINAPI TlsGetValue(DWORD dwTlsIndex);
WINBASEAPI BOOL WINAPI TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);
WINBASEAPI BOOL WINAPI TlsFree(DWORD dwTlsIndex);
WINBASEAPI BOOL WINAPI GetProcessTimes(HANDLE hProcess, LPFILETIME lpCreationTime, LPFILETIME lpExitTime, LPFILETIME lpKernelTime, LPFILETIME lpUserTime);
WINBASEAPI void WINAPI GetCurrentProcessorNumberEx(PPROCESSOR_NUMBER);

// from porcesstoplogyapi.h
typedef struct _GROUP_AFFINITY GROUP_AFFINITY, *PGROUP_AFFINITY;
WINBASEAPI BOOL WINAPI GetThreadGroupAffinity(HANDLE hThread, PGROUP_AFFINITY GroupAffinity);
WINBASEAPI BOOL WINAPI SetThreadGroupAffinity(HANDLE hThread, const GROUP_AFFINITY* GroupAffinity, PGROUP_AFFINITY PreviousGroupAffinity);

// from sysinfoapi.h
typedef struct _MEMORYSTATUSEX MEMORYSTATUSEX, *LPMEMORYSTATUSEX;
typedef struct _SYSTEM_INFO SYSTEM_INFO, *LPSYSTEM_INFO;
WINBASEAPI BOOL WINAPI GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpBuffer);
WINBASEAPI void WINAPI GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
WINBASEAPI void WINAPI GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime);
WINBASEAPI DWORD WINAPI GetTickCount(void);
WINBASEAPI ULONGLONG WINAPI GetTickCount64(void);

// from libloaderapi.h
WINBASEAPI FARPROC WINAPI GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
WINBASEAPI HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);
WINBASEAPI HMODULE WINAPI GetModuleHandleW(LPCWSTR lpModuleName);
#if defined(ISOLATION_AWARE_ENABLED) && ISOLATION_AWARE_ENABLED != 0
// LoadLibraryExW is #defined to be IsolationAwareLoadLibraryExW, which is an inline function in winbase.inl. That
// function is not replicated here; if you need it, you need to either #include <Windows.h> or replicate the inline
// function in your module or header file.
#else
WINBASEAPI HMODULE WINAPI LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
#endif
WINBASEAPI BOOL WINAPI FreeLibrary(HMODULE hLibModule);
WINBASEAPI BOOL WINAPI SetDefaultDllDirectories(DWORD DirectoryFlags);
typedef PVOID DLL_DIRECTORY_COOKIE, *PDLL_DIRECTORY_COOKIE;
WINBASEAPI BOOL WINAPI RemoveDllDirectory(DLL_DIRECTORY_COOKIE Cookie);
#define CARBWIN_LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR    0x00000100
#define CARBWIN_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS    0x00001000

WINBASEAPI BOOL WINAPI GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule);
#define CARBWIN_GET_MODULE_HANDLE_EX_FLAG_PIN                 (0x00000001)
#define CARBWIN_GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT  (0x00000002)
#define CARBWIN_GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS        (0x00000004)

WINBASEAPI DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
WINBASEAPI DWORD WINAPI GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);

// from memoryapi.h
WINBASEAPI HANDLE WINAPI CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName);
WINBASEAPI HANDLE WINAPI OpenFileMappingW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
WINBASEAPI LPVOID WINAPI MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
WINBASEAPI LPVOID WINAPI MapViewOfFileEx(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap, LPVOID lpBaseAddress);
WINBASEAPI BOOL WINAPI UnmapViewOfFile(LPCVOID lpBaseAddress);
WINBASEAPI LPVOID WINAPI VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
WINBASEAPI BOOL WINAPI VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

// from heapapi.h
WINBASEAPI HANDLE WINAPI GetProcessHeap(void);

// from fileapi.h
WINBASEAPI DWORD WINAPI GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart);
WINBASEAPI DWORD WINAPI GetFinalPathNameByHandleW(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
WINBASEAPI HANDLE WINAPI CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
WINBASEAPI DWORD WINAPI GetFileAttributesW(LPCWSTR lpFileName);
WINBASEAPI BOOL WINAPI LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped);
WINBASEAPI BOOL WINAPI UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped);
WINBASEAPI BOOL WINAPI DeleteFileA(LPCSTR lpFileName);
WINBASEAPI BOOL WINAPI DeleteFileW(LPCWSTR lpFileName);
WINBASEAPI BOOL WINAPI GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);

///////////////////////////////////////////////////////////////////////////////
// Header files below are not include with Windows.h

// from psapi.h
typedef struct _PROCESS_MEMORY_COUNTERS PROCESS_MEMORY_COUNTERS, *PPROCESS_MEMORY_COUNTERS;
BOOL WINAPI K32GetProcessMemoryInfo(HANDLE hProcess, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);

// from pathcch.h
WINBASEAPI HRESULT APIENTRY PathAllocCanonicalize(PCWSTR pszPathIn, ULONG dwFlags, PWSTR* ppszPathOut);

// from WinNls.h
WINBASEAPI int WINAPI LCMapStringW(LCID Locale, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc, LPWSTR lpDestStr, int cchDest);

#endif // NV_FORGE_WINDEF_H

// Undef temporary defines
#ifdef CARBWIN_WINBASEAPI_DEFINED
#undef WINBASEAPI
#undef CARBWIN_WINBASEAPI_DEFINED
#endif
#ifdef CARBWIN_WINAPI_DEFINED
#undef WINAPI
#undef CARBWIN_WINAPI_DEFINED
#endif
#ifdef CARBWIN_SHSTDAPI_DEFINED
#undef SHSTDAPI
#undef SHSTDAPI_
#undef CARBWIN_SHSTDAPI_DEFINED
#endif
#ifdef CARBWIN_APIENTRY_DEFINED
#undef APIENTRY
#undef CARBWIN_APIENTRY_DEFINED
#endif
#ifdef CARBWIN_WINADVAPI_DEFINED
#undef WINADVAPI
#undef CARBWIN_WINADVAPI_DEFINED
#endif

#ifdef __cplusplus
}
#endif

#endif
// clang-format on

CARB_IGNOREWARNING_MSC_POP
