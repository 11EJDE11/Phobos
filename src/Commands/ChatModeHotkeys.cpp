#include "ChatModeHotkeys.h"

#include <Utilities/GeneralUtils.h>

namespace ChatModeHotkeys
{
	const char* ChatToAllCommandClass::GetName() const
	{
		return ChatToAllCommandName;
	}

	const wchar_t* ChatToAllCommandClass::GetUIName() const
	{
		return GeneralUtils::LoadStringUnlessMissing("TXT_CHAT_TO_ALL", L"Chat to all players");
	}

	const wchar_t* ChatToAllCommandClass::GetUICategory() const
	{
		return CATEGORY_INTERFACE;
	}

	const wchar_t* ChatToAllCommandClass::GetUIDescription() const
	{
		return GeneralUtils::LoadStringUnlessMissing(
			"TXT_CHAT_TO_ALL_DESC",
			L"Open message input and set the send mode to all players."
		);
	}

	void ChatToAllCommandClass::Execute(WWKey eInput) const
	{
	}

	const char* ChatToAlliesCommandClass::GetName() const
	{
		return ChatToAlliesCommandName;
	}

	const wchar_t* ChatToAlliesCommandClass::GetUIName() const
	{
		return GeneralUtils::LoadStringUnlessMissing("TXT_CHAT_TO_ALLIES", L"Chat to allies");
	}

	const wchar_t* ChatToAlliesCommandClass::GetUICategory() const
	{
		return CATEGORY_INTERFACE;
	}

	const wchar_t* ChatToAlliesCommandClass::GetUIDescription() const
	{
		return GeneralUtils::LoadStringUnlessMissing(
			"TXT_CHAT_TO_ALLIES_DESC",
			L"Open message input and set the send mode to allied players."
		);
	}

	void ChatToAlliesCommandClass::Execute(WWKey eInput) const
	{
	}
}
