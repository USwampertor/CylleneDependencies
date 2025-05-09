#pragma once

#include "DialogsDesc.h"

namespace xwin
{

MessageResponse showMessageBox(const MessageDesc& mdesc);

bool showOpenDialog(const OpenSaveDialogDesc& odesc, std::string& outPath);

bool showSaveDialog(const OpenSaveDialogDesc& sdesc, std::string& outPath);

}