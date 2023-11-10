// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/Interface.h>


namespace omni
{
namespace kit
{


/*!
 * Message box type.
 */
enum class MessageBoxType
{
    eInfo,
    eWarning,
    eError,
    eQuestion
};

/*!
 * Buttons to place on the message box.
 */
enum class MessageBoxButtons
{
    eOk,
    eOkCancel,
    eYesNo
};

/*!
 * Message box user response.
 */
enum class MessageBoxResponse
{
    eOk,
    eCancel,
    eYes,
    eNo,
    eNone,
    eError
};

class IAppMessageBox
{
public:
    CARB_PLUGIN_INTERFACE("omni::kit::IAppMessageBox", 1, 1);

    /**
     * Show OS specific message box.
     *
     * This is a blocking call that will return user response to the message box.
     */
    virtual MessageBoxResponse show(const char* message,
                                    const char* title,
                                    MessageBoxType type,
                                    MessageBoxButtons buttons) = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                Inline Functions                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


} // namespace kit
} // namespace omni
