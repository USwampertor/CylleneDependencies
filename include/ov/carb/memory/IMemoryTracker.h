// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "MemoryTrackerDefines.h"
#include "MemoryTrackerReplaceAllocation.h"
#include "MemoryTrackerTypes.h"

#if CARB_MEMORY_WORK_AS_PLUGIN
#    include "../Framework.h"
#endif
#include "../Types.h"

namespace carb
{
namespace memory
{
/**
 * Defines a toolkit Memory Tracker, used to monitor/track memory usage/leak.
 */
struct IMemoryTracker
{
    CARB_PLUGIN_INTERFACE("carb::memory::IMemoryTracker", 1, 0)

    /**
     * Setting this number either in the debugger, or in code will result in causing
     * the memory allocator to break when this allocation is encountered.
     */
    intptr_t* breakOnAlloc;

    /**
     * Specify that the debugger signal should be triggered nth allocation within a context.
     * @param context The context to modify.
     * @param nAlloc Signal the debugger on the nth allocation within context. -1 disables
     *                this feature.
     * This feature only respects the top of the Context stack.
     */
    void(CARB_ABI* contextBreakOnAlloc)(const Context& context, intptr_t nAlloc);

    /**
     * Makes the context active on the context stack for this thread.
     *
     * @param context The context to become active
     */
    void(CARB_ABI* pushContext)(const Context& context);

    /**
     * Pops the context on the top of the stack off for this thread.
     */
    void(CARB_ABI* popContext)();

    /**
     * Creates an allocation group.
     *
     * @param name The name of the memory address group.
     * @return The address group object.
     */
    AllocationGroup*(CARB_ABI* createAllocationGroup)(const char* name);

    /**
     * Destroys an allocation group.
     *
     * @param allocationGroup The address group to destroy
     */
    void(CARB_ABI* destroyAllocationGroup)(AllocationGroup* allocationGroup);

    /**
     * Records an allocation on behalf of a region.
     *
     * The context recorded is on the top of the context stack. Additionally, the backtrace
     * associated with this allocation is recorded from this call site.
     *
     * @param allocationGroup The allocationGroup to record the allocation into
     * @param address The address that the allocation exists at
     * @param size The size of the allocation.
     */
    void(CARB_ABI* recordAllocation)(AllocationGroup* allocationGroup, const void* const address, size_t size);

    /**
     * Records an allocation on behalf of a region.
     *
     * Additionally, the backtrace associated with this allocation is recorded from this call
     * site.
     *
     * @param allocationGroup The allocationGroup to record the allocation into
     * @param context The context that the allocation is associated with.
     * @param address The address that the allocation exists at
     * @param size The size of the allocation.
     */
    void(CARB_ABI* recordAllocationWithContext)(AllocationGroup* allocationGroup,
                                                const Context& context,
                                                const void* const address,
                                                size_t size);

    /**
     * Records that an allocation that was previously recorded was released.
     *
     * @param allocationGroup The allocation group that the allocation was associated with.
     * @param address The address the allocation was associated with.
     */
    void(CARB_ABI* recordFree)(AllocationGroup* allocationGroup, const void* const address);

    /**
     * Creates a bookmark of the current state of the memory system.
     *
     * This is somewhat of a heavy-weight operation and should only be used at certain times
     * such as level load.
     *
     * @return A snapshot of the current state of the memory system.
     */
    Bookmark*(CARB_ABI* createBookmark)();

    /**
     * Destroys a memory bookmark.
     *
     * @param bookmark The bookmark to destroy.
     */
    void(CARB_ABI* destroyBookmark)(Bookmark* bookmark);

    /**
     * Get a basic summary of the current state of the memory system, that is of a low enough overhead that we could put
     * on a ImGui page that updates ever frame.
     *
     * @return The Summary struct of current state.
     */
    Summary(CARB_ABI* getSummary)();

    /**
     * Generates a memory report.
     *
     * @param reportFlags The flags about the report.
     * @param report The generated report, it is up to the user to release the report with releaseReport.
     * @return nullptr if the report couldn't be generated, otherwise the report object.
     */
    Report*(CARB_ABI* createReport)(ReportFlags reportFlags);

    /**
     * Generates a memory report, starting at a bookmark to now.
     *
     * @param reportFlags The flags about the report.
     * @param bookmark Any allocations before bookmark will be ignored in the report.
     * @param report The generated report, it is up to the user to release the report with
     *              releaseReport.
     * @return nullptr if the report couldn't be generated, otherwise the report object.
     */
    Report*(CARB_ABI* createReportFromBookmark)(ReportFlags reportFlags, Bookmark* bookmark);

    /**
     * Frees underlying data for the report.
     *
     * @param report The report to free.
     */
    void(CARB_ABI* destroyReport)(Report* report);

    /**
     * Returns a pointer to the report data. The returned pointer can not be stored for persistence usage, and it will
     * be freed along with the report.
     *
     * @param report The report data to inspect.
     * @return The raw report data.
     */
    const char*(CARB_ABI* reportGetData)(Report* report);

    /**
     * Returns the number of leaks stored in a memory report.
     *
     * @param report The report to return the number of leaks for.
     * @return The number of leaks associated with the report.
     */
    size_t(CARB_ABI* getReportMemoryLeakCount)(const Report* report);

    /**
     * When exiting, memory tracker will create a memory leak report.
     * The report file name could be (from high priority to low):
     * - In command line arguments (top priority) as format: --memory.report.path
     * - Parameter in this function
     * - The default: * ${WorkingDir}/memoryleak.json
     * @param fileName The file name (including full path) to save memory leak report when exiting
     */
    void(CARB_ABI* setReportFileName)(const char* fileName);
};
} // namespace memory
} // namespace carb

#if CARB_MEMORY_WORK_AS_PLUGIN
CARB_WEAKLINK carb::memory::IMemoryTracker* g_carbMemoryTracker;
#    define CARB_MEMORY_TRACKER_GLOBALS()
#endif

namespace carb
{
namespace memory
{
#if CARB_MEMORY_WORK_AS_PLUGIN
inline void registerMemoryTrackerForClient()
{
    Framework* framework = getFramework();
    g_carbMemoryTracker = framework->acquireInterface<memory::IMemoryTracker>();
}
inline void deregisterMemoryTrackerForClient()
{
    g_carbMemoryTracker = nullptr;
}
/**
 * Get the toolkit Memory Tracker
 * @return the memory tracker toolkit
 */
inline IMemoryTracker* getMemoryTracker()
{
    return g_carbMemoryTracker;
}

#else
/**
 * Get the toolkit Memory Tracker
 * @return the memory tracker toolkit
 */
CARB_EXPORT memory::IMemoryTracker* getMemoryTracker();
#endif

/**
 * RAII Context helper
 *
 * This class uses RAII to automatically set a context as active and then release it.
 *
 * @code
 * {
 *   ScopedContext(SoundContext);
 *   // Allocate some sound resources
 * }
 * @endcode
 */
class ScopedContext
{
public:
    ScopedContext(const Context& context)
    {
        CARB_UNUSED(context);
#if CARB_MEMORY_TRACKER_ENABLED
        IMemoryTracker* tracker = getMemoryTracker();
        CARB_ASSERT(tracker);
        if (tracker)
            tracker->pushContext(context);
#endif
    }
    ~ScopedContext()
    {
#if CARB_MEMORY_TRACKER_ENABLED
        IMemoryTracker* tracker = getMemoryTracker();
        CARB_ASSERT(tracker);
        if (tracker)
            tracker->popContext();
#endif
    }
};
} // namespace memory
} // namespace carb
