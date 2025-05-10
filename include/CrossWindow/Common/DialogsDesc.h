#pragma once

#include <string>
#include <vector>

namespace xwin
{

enum class MessageButtons
{
  OK,
  OKCancel,
  YesNo,
  YesNoCancel,
  RetryCancel,
  AbortRetryIgnore
};

enum class MessageIcon
{
  None,
  Information,
  Warning,
  Error,
  Question
};

enum class MessageDefaultButton
{
  Button1,
  Button2,
  Button3
};

enum class MessageResponse
{
  OK,
  Cancel,
  Yes,
  No,
  Retry,
  Abort,
  Ignore
};

struct MessageDesc
{
  std::string title;
  std::string content;
  MessageButtons buttons = MessageButtons::OK;
  MessageIcon icon = MessageIcon::None;
  MessageDefaultButton defaultButton = MessageDefaultButton::Button1;
};

struct OpenSaveDialogDesc
{
	// Title of the dialog
    std::string title;

	// Label used by the Accept button of the dialog
	std::string okLabel;

	// Whether to only interact with folders (for open dialog only)
	bool openFolders = false;

	// Pairs of (File Description, ["jpg", "png", "myFileType"])
    std::vector<std::pair<std::string, std::vector<std::string>>> filters;
};
}