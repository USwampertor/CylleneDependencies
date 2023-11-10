// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The audio group interface.
 */
#pragma once

#include "../Interface.h"
#include "AudioTypes.h"
#include "IAudioData.h"
#include "IAudioPlayback.h"


namespace carb
{
namespace audio
{

/************************************* Interface Objects *****************************************/
/** an object containing zero or more sound data objects.  This group may be used to hold sounds
 *  that can be selected with differing probabilities when trying to play a high level sound clip
 *  or to only select a specific sound as needed.  A group may contain any number of sounds.
 */
struct Group;


/******************************** typedefs, enums, & macros **************************************/
/** base type for the flags that control the behavior of the creation of a group. */
typedef uint32_t GroupFlags;

/** group creation flag to indicate that the random number generator for the group should
 *  be seeded with a fixed constant instead of another random value.  This will cause the
 *  group's random number sequence to be repeatable on each run instead of random.  Note that
 *  the constant seed may be platform or implementation dependent.  This is useful for tests
 *  where a stable but non-consecutive sequence is needed.  Note that each group has its own
 *  random number stream and choosing a random sound from one group will not affect the
 *  random number stream of any other group.
 */
constexpr GroupFlags fGroupFlagFixedSeed = 0x00000001;

/** an entry in a table of sounds being added to a sound group on creation or a single sound
 *  being added to a sound group with a certain region to be played.  This can be used to
 *  provide sound atlas support in a sound group.  Each (or some) of the sounds in the group
 *  can be the same, but each only plays a small region instead of the full sound.
 */
struct SoundEntry
{
    /** the sound data object to add to the sound group.  This must not be nullptr.  A reference
     *  will be taken to this sound data object when it is added to the group.
     */
    SoundData* sound;

    /** the starting point for playback of the new sound.  This value is interpreted in the units
     *  specified in @ref playUnits.  This should be 0 to indicate the start of the sound data object
     *  as the starting point.  This may not be larger than the valid data length (in the same
     *  units) of the sound data object itself.
     */
    uint64_t playStart = 0;

    /** the length of data to play in the sound data object.  This extends from the @ref playStart
     *  point extending through this much data measured in the units @ref playUnits.  This should
     *  be 0 to indicate that the remainder of the sound data object starting from @ref playStart
     *  should be played.
     */
    uint64_t playLength = 0;

    /** the units to interpret the @ref playStart and @ref playLength values in.  Note that using
     *  some time units may not provide precise indexing into the sound data object.  Also note
     *  that specifying this offset in bytes often does not make sense for compressed data.
     */
    UnitType playUnits = UnitType::eFrames;
};

/** descriptor of a new group to be created.  A group may be optionally named and optionally
 *  created with a set of sound data objects initially added to it.
 */
struct GroupDesc
{
    /** flags to control the behavior of the group's creation or behavior.  This is zero or
     *  more of the kGroupFlag* flags.
     */
    GroupFlags flags = 0;

    /** optional name to initially give to the group.  This can be changed at any later point
     *  with setGroupName().  The name has no functional purpose except to identify the group
     *  to a user.
     */
    const char* name = nullptr;

    /** the total number of sound data objects in the @ref initialSounds table. */
    size_t count = 0;

    /** a table of sounds and regions that should be added to the new group immediately on
     *  creation.  This may be nullptr to create an empty group, or this may be a table of
     *  @ref count sound data objects and regions to be added to the group.  When each sound
     *  is added to the group, a reference to the object will be taken.  The reference will
     *  be released when the sound is removed from the group or the group is destroyed.  The
     *  sound data object will only be destroyed when removed from the group or the group is
     *  destroyed if the group owned the last reference to it.
     */
    SoundEntry* initialSounds = nullptr;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/** names of possible methods for choosing sounds to play from a sound group.  These are used with
 *  the chooseSound() function.  The probabilities of each sound in the group are only used when
 *  the @ref ChooseType::eRandom selection type is used.
 */
enum class ChooseType
{
    /** choose a sound from the group at random using each sound's relative probabilities to
     *  perform the selection.  By default, all sounds in a group will have a uniform probability
     *  distribution.  The tendency to have one sound selected over others can be changed by
     *  changing that sound's probability with setProbability().
     */
    eRandom,

    /** chooses the next sound in the group.  The next sound is either the first sound in the
     *  group if none has been selected yet, or the sound following the one that was most recently
     *  selected from the group.  Even if another selection type was used in a previous call,
     *  this will still return the sound after the one that was most recently selected.  This
     *  will wrap around to the first sound in the group if the last sound in the group was
     *  previously selected.
     */
    eNext,

    /** chooses the previous sound in the group.  The previous sound is either the last sound in
     *  the group if none has been selected yet, or the sound preceding the one that was most
     *  recently selected from the group.  Even if another selection type was used in a previous
     *  call, this will still return the sound before the one that was most recently selected.
     *  This will wrap around to the last sound in the group if the first sound in the group was
     *  previously selected.
     */
    ePrevious,

    /** always chooses the first sound in the group. */
    eFirst,

    /** always chooses the last sound in the group. */
    eLast,
};

/** used in the @ref ProbabilityDesc object to indicate that all sounds within a group should
 *  be affected, not just a single index.
 */
constexpr size_t kGroupIndexAll = ~0ull;

/** used to identify an invalid index in the group or that a sound could not be added. */
constexpr size_t kGroupIndexInvalid = static_cast<size_t>(-2);

/** descriptor for specifying the relative probabilities for choosing one or more sounds in a
 *  sound group.  This allows the probabilities for a sound within a group being chosen at play
 *  time.  By default, a sound group assigns equal probabilities to all of its members.
 */
struct ProbabilityDesc
{
    /** set to the index of the sound within the group to change the probability for.  This may
     *  either be @ref kGroupIndexAll to change all probabilities within the group, or the
     *  zero based index of the single sound to change.  When @ref kGroupIndexAll is used,
     *  the @ref probability value is ignored since a uniform distribution will always be set
     *  for each sound in the group.  If this index is outside of the range of the number of
     *  sounds in the group, this call will silently fail.
     */
    size_t index;

    /** the new relative probability value to set for the specified sound in the group.  This
     *  value will be ignored if the @ref index value is @ref kGroupIndexAll however.  This
     *  value does not need to be within any given range.  This simply specifies the relative
     *  frequency of the specified sound being selected compared to other sounds in the group.
     *  Setting this to 0 will cause the sound to never be selected from the group.
     */
    float probability;

    /** value reserved for future expansion.  This should be set to nullptr. */
    void* ext = nullptr;
};


/******************************** Audio Sound Group Management Interface *******************************/
/** Sound group management interface.
 *
 *  See these pages for more detail:
 *  @rst
    * :ref:`carbonite-audio-label`
    * :ref:`carbonite-audio-group-label`
    @endrst
 */
struct IAudioGroup
{
    CARB_PLUGIN_INTERFACE("carb::audio::IAudioGroup", 1, 0)

    /** creates a new sound group.
     *
     *  @param[in] desc     a descriptor of the new group to be created.  This may be nullptr to
     *                      create a new, empty, unnamed group.
     *  @returns the new group object if successfully created.  This must be destroyed with a call
     *           to destroyGroup() when it is no longer needed.
     *  @returns nullptr if the new group could not be created.
     *
     *  @remarks This creates a new sound group object.  A sound group may contain zero or more
     *           sound data objects.  The group may be initially populated by one or more sound
     *           data objects that are specified in the descriptor or it may be created empty.
     *
     *  @note Access to the group object is not thread safe.  It is the caller's responsibility
     *        to ensure that all accesses that may occur simultaneously are properly protected
     *        with a lock.
     */
    Group*(CARB_ABI* createGroup)(const GroupDesc* desc);

    /** destroys a sound group.
     *
     *  @param[in] group    the group to be destroyed.  This must not be nullptr.
     *  @returns no return value.
     *
     *  @remarks This destroys a sound group object.  Each sound data object in the group at
     *           the time of destruction will have one reference removed from it.  The group
     *           object will no longer be valid upon return.
     */
    void(CARB_ABI* destroyGroup)(Group* group);

    /** retrieves the number of sound data objects in a group.
     *
     *  @param[in] group    the group to retrieve the sound data object count for.  This must
     *                      not be nullptr.
     *  @returns the total number of sound data objects in the group.
     *  @returns 0 if the group is empty.
     */
    size_t(CARB_ABI* getSize)(const Group* group);

    /** retrieves the name of a group.
     *
     *  @param[in] group    the group to retrieve the name for.  This must not be nullptr.
     *  @returns the name of the group.
     *  @returns nullptr if the group does not have a name.
     *
     *  @remarks This retrieves the name of a group.  The returned string will be valid until
     *           the group's name is changed with setGroupName() or the group is destroyed.  It
     *           is highly recommended that the returned string be copied if it needs to persist.
     */
    const char*(CARB_ABI* getName)(const Group* group);

    /** sets the new name of a group.
     *
     *  @param[in] group    the group to set the name for.  This must not be nullptr.
     *  @param[in] name     the new name to set for the group.  This may be nullptr to remove
     *                      the group's name.
     *  @returns no return value.
     *
     *  @remarks This sets the new name for a group.  This will invalidate any names that were
     *           previously returned from getGroupName() regardless of whether the new name is
     *           different.
     */
    void(CARB_ABI* setName)(Group* group, const char* name);

    /** adds a new sound data object to a group.
     *
     *  @param[in] group    the group to add the new sound data object to.  This must not be
     *                      nullptr.
     *  @param[in] sound    the sound data object to add to the group.  This must not be nullptr.
     *  @returns the index of the new sound in the group if it is successfully added.
     *  @retval kGroupIndexInvalid if the new sound could not be added to the group.
     *
     *  @remarks This adds a new sound data object to a group.  The group will take a reference
     *           to the sound data object when it is successfully added.  There will be no
     *           checking to verify that the sound data object is not already a member of the
     *           group.  The initial relative probability for any new sound added to a group
     *           will be 1.0.  This may be changed later with setProbability().
     *
     *  @note This returned index is only returned for the convenience of immediately changing the
     *        sound's other attributes within the group (ie: the relative probability).  This
     *        index should not be stored for extended periods since it may be invalidated by any
     *        calls to removeSound*().  If changes to a sound in the group need to be made at a
     *        later time, the index should either be known ahead of time (ie: from a UI that is
     *        tracking the group's state), or the group's members should be enumerated to first
     *        find the index of the desired sound.
     */
    size_t(CARB_ABI* addSound)(Group* group, SoundData* sound);

    /** adds a new sound data object with a play region to a group.
     *
     *  @param[in] group    the group to add the new sound data object to.  This must not be
     *                      nullptr.
     *  @param[in] sound    the sound data object and play range data to add to the group.
     *                      This must not be nullptr.
     *  @returns the index of the new sound in the group if it is successfully added.
     *  @retval kGroupIndexInvalid if the new sound could not be added to the group.
     *
     *  @remarks This adds a new sound data object with a play range to a group.  The group
     *           will take a reference to the sound data object when it is successfully added.
     *           There will be no checking to verify that the sound data object is not already
     *           a member of the group.  The play region for the sound may indicate the full
     *           sound or only a small portion of it.  The initial relative probability for any
     *           new sound added to a group will be 1.0.  This may be changed later with
     *           setProbability().
     *
     *  @note This returned index is only returned for the convenience of immediately changing the
     *        sound's other attributes within the group (ie: the relative probability).  This
     *        index should not be stored for extended periods since it may be invalidated by any
     *        calls to removeSound*().  If changes to a sound in the group need to be made at a
     *        later time, the index should either be known ahead of time (ie: from a UI that is
     *        tracking the group's state), or the group's members should be enumerated to first
     *        find the index of the desired sound.
     */
    size_t(CARB_ABI* addSoundWithRegion)(Group* group, const SoundEntry* sound);

    /** removes a sound data object from a group.
     *
     *  @param[in] group    the group to remove the sound from.  This must not be nullptr.
     *  @param[in] sound    the sound to be removed from the group.  This may be nullptr to
     *                      remove all sound data objects from the group.
     *  @returns true if the sound is a member of the group and it is successfully removed.
     *  @returns false if the sound is not a member of the group.
     *
     *  @remarks This removes a single sound data object from a group.  Only the first instance
     *           of the requested sound will be removed from the group.  If the sound is present
     *           in the group multiple times, additional explicit calls to remove the sound must
     *           be made to remove all of them.
     *
     *  @note Once a sound is removed from a group, the ordering of sounds within the group may
     *        change.  The relative probabilities of each remaining sound will still be
     *        unmodified.
     */
    bool(CARB_ABI* removeSound)(Group* group, SoundData* sound);

    /** removes a sound data object from a group by its index.
     *
     *  @param[in] group    the group to remove the sound from.  This must not be nullptr.
     *  @param[in] index    the zero based index of the sound to remove from the group.  This
     *                      may be @ref kGroupIndexAll to clear the entire group.  This must not
     *                      be @ref kGroupIndexInvalid.
     *  @returns true if the sound is a member of the group and it is successfully removed.
     *  @returns false if the given index is out of range of the size of the group.
     *
     *  @note Once a sound is removed from a group, the ordering of sounds within the group may
     *        change.  The relative probabilities of each remaining sound will still be
     *        unmodified.
     */
    bool(CARB_ABI* removeSoundAtIndex)(Group* group, size_t index);

    /** sets the current sound play region for an entry in the group.
     *
     *  @param[in] group    the group to modify the play region for a sound in.  This must not
     *                      be nullptr.
     *  @param[in] index    the zero based index of the sound entry to update the region for.
     *                      This must not be @ref kGroupIndexInvalid or @ref kGroupIndexAll.
     *  @param[in] region   the specification of the new region to set on the sound.  The
     *                      @a sound member will be ignored and assumed that it either matches
     *                      the sound data object already at the given index or is nullptr.
     *                      All other members must be valid.  This must not be nullptr.
     *  @returns true if the play region for the selected sound is successfully updated.
     *  @returns false if the index was out of range of the size of the group.
     *
     *  @remarks This modifies the play region values for a single sound entry in the group.
     *           This will not replace the sound data object at the requested entry.  Only
     *           the play region (start, length, and units) will be updated for the entry.
     *           It is the caller's responsibility to ensure the new play region values are
     *           within the range of the sound data object's current valid region.
     */
    bool(CARB_ABI* setSoundRegion)(Group* group, size_t index, const SoundEntry* region);

    /** retrieves the sound data object at a given index in a group.
     *
     *  @param[in] group    the group to retrieve a sound from.  This must not be nullptr.
     *  @param[in] index    the index of the sound data object to retrieve.  This must not be
     *                      @ref kGroupIndexInvalid or @ref kGroupIndexAll.
     *  @returns the sound data object at the requested index in the group.  An extra reference
     *           to this object will not be taken and therefore does not have to be released.
     *           This object will be valid as long as it is still a member of the group.
     *  @returns nullptr if the given index was out of range of the size of the group.
     */
    SoundData*(CARB_ABI* getSound)(const Group* group, size_t index);

    /** retrieves the sound data object and region information at a given index in a group.
     *
     *  @param[in] group    the group to retrieve a sound from.  This must not be nullptr.
     *  @param[in] index    the index of the sound data object information to retrieve.  This
     *                      must not be @ref kGroupIndexInvalid or @ref kGroupIndexAll.
     *  @param[out] entry   receives the information for the sound entry at the given index in
     *                      the group.  This not be nullptr.
     *  @returns true if the sound data object and its region information are successfully
     *           retrieved.  The sound data object returned in @p entry will not have an extra
     *           reference taken to it and does not need to be released.
     *  @returns false if the given index was out of range of the group.
     */
    bool(CARB_ABI* getSoundEntry)(const Group* group, size_t index, SoundEntry* entry);

    /** sets the new relative probability for a sound being selected from a sound group.
     *
     *  @param[in] group    the sound group to change the relative probabilities for.  This must
     *                      not be nullptr.
     *  @param[in] desc     the descriptor of the sound within the group to be changed and the
     *                      new relative probability for it.  This must not be nullptr.
     *  @returns no return value.
     *
     *  @remarks This sets the new relative probability for choosing a sound within a sound group.
     *           Each sound in the group gets a relative probability of 1 assigned to it when it
     *           is first added to the group (ie: giving a uniform distribution initially).  These
     *           relative probabilities can be changed later by setting a new value for individual
     *           sounds in the group.  The actual probability of a particular sound being chosen
     *           from the group depends on the total sum of all relative probabilities within the
     *           group as a whole.  For example, if a group of five sounds has been assigned the
     *           relative probabilities 1, 5, 7, 6, and 1, there is a 1/20 chance of the first or
     *           last sounds being chosen, a 1/4 chance of the second sound being chosen, a 7/20
     *           chance of the third sound being chosen, and a 6/20 chance of the fourth sound
     *           being chosen.
     */
    void(CARB_ABI* setProbability)(Group* group, const ProbabilityDesc* desc);

    /** retrieves a relative probability for a sound being selected from a sound group.
     *
     *  @param[in] group    the group to retrieve a relative probability for.
     *  @param[in] index    the index of the sound in the group to retrieve the relative
     *                      probability for.  If this is out of range of the size of the
     *                      group, the call will fail.  This must not be @ref kGroupIndexAll
     *                      or @ref kGroupIndexInvalid.
     *  @returns the relative probability of the requested sound within the group.
     *  @returns 0.0 if the requested index was out of range of the group's size.
     *
     *  @remarks This retrieves the relative probability of the requested sound within a group
     *           being chosen by the chooseSound() function when using the ChooseType::eRandom
     *           selection type.  Note that this will always the relative probability value that
     *           was either assigned when the sound was added to the group (ie: 1.0) or the one
     *           that was most recently set using a call to the setProbability() function.
     *
     *  @note This is intended to be called in an editor situation to retrieve the relative
     *        probability values that are currently set on a group for display purposes.
     */
    float(CARB_ABI* getProbability)(const Group* group, size_t index);

    /** gets the relative probability total for all sounds in the group.
     *
     *  @param[in] group    the group to retrieve the relative probabilities total for.  This
     *                      must not be nullptr.
     *  @returns the sum total of the relative probabilities of each sound in the group.
     *  @returns 0.0 if the group is empty or all sounds have a zero relative probability.
     *           It is the caller's responsibility to check for this before using it as a
     *           divisor.
     *
     *  @remarks This retrieves the total of all relative probabilities for all sounds in a
     *           group.  This can be used to calculate the absolute probability of each sound
     *           in the group.  This is done by retrieving each sound's relative probability
     *           with getProbability(), then dividing it by the value returned here.
     */
    float(CARB_ABI* getProbabilityTotal)(const Group* group);

    /** chooses a sound from a sound group.
     *
     *  @param[in] group    the group to select a sound from.  This must not be nullptr.
     *  @param[in] type     the specific algorithm to use when choosing the sound.
     *  @param[out] play    receives the play descriptor for the chosen sound.  On success, this
     *                      will be filled in with enough information to play the chosen sound and
     *                      region once as a non-spatial sound.  It is the caller's responsibility
     *                      to fill in any additional parameters (ie: voice callback function,
     *                      additional voice parameters, spatial sound information, etc).  This
     *                      must not be nullptr.  This object is assumed to be uninitialized and
     *                      all members will be filled in.
     *  @returns true if a sound if chosen and the play descriptor @p play is valid.
     *  @returns false if the group is empty.
     *  @returns false if the maximum number of sound instances from this group are already
     *           playing.  This may be tried again later and will succeed when the playing
     *           instance count drops below the limit.
     *
     *  @remarks This chooses a sound from a group according to the given algorithm.  When
     *           choosing a random sound, the sound is chosen using the relative probabilities
     *           of each of the sounds in the group.  When choosing the next or previous sound,
     *           the sound in the group either after or before the last one that was most recently
     *           returned from chooseSound() will be returned.  This will never fail unless the
     *           group is empty.
     */
    bool(CARB_ABI* chooseSound)(Group* group, ChooseType type, PlaySoundDesc* play);

    /** retrieves the maximum simultaneously playing instance count for sounds in a group.
     *
     *  @param[in] group    the group to retrieve the maximum instance count for.  This must not
     *                      be nullptr.
     *  @returns the maximum instance count for the group if it is limited.
     *  @retval kInstancesUnlimited if the instance count is unlimited.
     *
     *  @remarks This retrieves the current maximum instance count for the sounds in a group.
     *           This limit is used to prevent too many instances of sounds in this group from
     *           being played simultaneously.  With the limit set to unlimited, playing too many
     *           instances can result in serious performance penalties and serious clipping
     *           artifacts caused by too much constructive interference.
     */
    uint32_t(CARB_ABI* getMaxInstances)(const Group* group);

    /** sets the maximum simultaneously playing instance count for sounds in a group.
     *
     *  @param[in] group    the group to change the maximum instance count for.  This must not be
     *                      nullptr.
     *  @param[in] limit    the new maximum instance limit for this sound group.  This may be
     *                      @ref kInstancesUnlimited to remove the limit entirely.
     *  @returns no return value.
     *
     *  @remarks This sets the new maximum playing instance count for sounds in a group.  This
     *           limit only affects the results of chooseSound().  When the limit is exceeded,
     *           calls to chooseSound() will start failing until some sound instances in the
     *           group finish playing.  This instance limit is also separate from the maximum
     *           instance count for each sound in the group.  Individual sound data objects
     *           also have their own maximum instance counts and will limit themselves when
     *           they are attempted to be played.  Note that these two limits may however
     *           interact with each other if the group's instance limit is not hit but the
     *           instance limit for the particular chosen sound has been reached.  It is the
     *           caller's responsibility to ensure the various instance limits are set in such
     *           a way this interaction is minimized.
     */
    void(CARB_ABI* setMaxInstances)(Group* group, uint32_t limit);

    /** retrieves the user data pointer for a sound group object.
     *
     *  @param[in] group    the sound group to retrieve the user data pointer for.  This must
     *                      not be nullptr.
     *  @returns the stored user data pointer.
     *  @returns nullptr if no user data has been set on the requested sound group.
     *
     *  @remarks This retrieves the user data pointer for the requested sound group.  This
     *           is used to associate any arbitrary data with a sound group object.  It is
     *           the caller's responsibility to ensure access to data is done in a thread
     *           safe manner.
     */
    void*(CARB_ABI* getUserData)(const Group* group);

    /** sets the user data pointer for a sound group.
     *
     *  @param[in] group    the sound group to set the user data pointer for.  This must not
     *                      be nullptr.
     *  @param[in] userData the new user data pointer to set.  This may include an optional
     *                      destructor if the user data object needs to be cleaned up.  This
     *                      may be nullptr to indicate that the user data pointer should be
     *                      cleared out entirely and no new object stored.
     *  @returns no return value.
     *
     *  @remarks This sets the user data pointer for the given sound group.  This is used to
     *           associate any arbitrary data with a sound group.  It is the caller's
     *           responsibility to ensure access to this table is done in a thread safe manner.
     *
     *  @note The sound group that this user data object is attached to must not be accessed
     *        from the destructor.  If the sound group is being destroyed when the user data
     *        object's destructor is being called, its contents will be undefined.
     */
    void(CARB_ABI* setUserData)(Group* group, const UserData* userData);
};

} // namespace audio
} // namespace carb
