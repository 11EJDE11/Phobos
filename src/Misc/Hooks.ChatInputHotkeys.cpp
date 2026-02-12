#include <Helpers/Macro.h>

#include <CommandClass.h>

#include <Commands/ChatModeHotkeys.h>

namespace
{
	enum class ChatSendMode : int
	{
		None = 0,
		All = 1,
		Allies = 2,
		PageUser = 3
	};

	struct ChatHotkeyCache
	{
		int CommandCount = -1;
		int HotkeyCount = -1;
		unsigned short ChatToAll = 0x000D;
		unsigned short ChatToAllies = 0x0008;
		unsigned short PageUser = 0x005C;
	};

	ChatHotkeyCache Cache {};

	constexpr unsigned short NormalizeMask = 0xE8FF;

	unsigned short NormalizeHotkey(unsigned short key)
	{
		return key & NormalizeMask;
	}

	CommandClass* FindCommandByName(const char* name)
	{
		for (int idx = 0; idx < CommandClass::Array.Count; ++idx)
		{
			auto* pCommand = CommandClass::Array.Items[idx];

			if (pCommand && !_strcmpi(pCommand->GetName(), name))
			{
				return pCommand;
			}
		}

		return nullptr;
	}

	bool TryGetHotkeyForCommandName(const char* commandName, unsigned short& outHotkey)
	{
		auto* pCommand = FindCommandByName(commandName);

		if (!pCommand || !CommandClass::Hotkeys.IndexTable || CommandClass::Hotkeys.IndexCount <= 0)
		{
			return false;
		}

		for (int idx = 0; idx < CommandClass::Hotkeys.IndexCount; ++idx)
		{
			const auto& entry = CommandClass::Hotkeys.IndexTable[idx];

			if (entry.Data == pCommand)
			{
				outHotkey = entry.ID;
				return true;
			}
		}

		return false;
	}

	void RefreshChatHotkeys()
	{
		if (Cache.CommandCount == CommandClass::Array.Count
			&& Cache.HotkeyCount == CommandClass::Hotkeys.IndexCount)
		{
			return;
		}

		Cache.CommandCount = CommandClass::Array.Count;
		Cache.HotkeyCount = CommandClass::Hotkeys.IndexCount;
		Cache.ChatToAll = 0x000D;
		Cache.ChatToAllies = 0x0008;
		Cache.PageUser = 0x005C;

		unsigned short configuredHotkey = 0;

		if (TryGetHotkeyForCommandName(ChatModeHotkeys::ChatToAllCommandName, configuredHotkey) && configuredHotkey)
		{
			Cache.ChatToAll = configuredHotkey;
		}

		if (TryGetHotkeyForCommandName(ChatModeHotkeys::ChatToAlliesCommandName, configuredHotkey) && configuredHotkey)
		{
			Cache.ChatToAllies = configuredHotkey;
		}

		if (TryGetHotkeyForCommandName(ChatModeHotkeys::PageUserCommandName, configuredHotkey) && configuredHotkey)
		{
			Cache.PageUser = configuredHotkey;
		}
	}

	ChatSendMode GetChatSendModeFromHotkey(unsigned short inputHotkey)
	{
		RefreshChatHotkeys();

		const auto normalizedInput = NormalizeHotkey(inputHotkey);

		if (normalizedInput == NormalizeHotkey(Cache.ChatToAll))
		{
			return ChatSendMode::All;
		}

		if (normalizedInput == NormalizeHotkey(Cache.ChatToAllies))
		{
			return ChatSendMode::Allies;
		}

		if (normalizedInput == NormalizeHotkey(Cache.PageUser))
		{
			return ChatSendMode::PageUser;
		}

		return ChatSendMode::None;
	}

	int ChatModeToLegacyAscii(ChatSendMode mode)
	{
		switch (mode)
		{
		case ChatSendMode::All:
			return 0x0D;
		case ChatSendMode::Allies:
			return 0x08;
		case ChatSendMode::PageUser:
			return 0x5C;
		default:
			return 0;
		}
	}
}

DEFINE_HOOK(0x55E4A3, MessageInput_HandleHotkeysAndSubmit_UseHotkeyRegistry, 0x26)
{
	GET(const unsigned int, keyInput, EDX);

	const auto mode = GetChatSendModeFromHotkey(static_cast<unsigned short>(keyInput & 0xFFFF));

	if (mode == ChatSendMode::None)
	{
		return 0x55E615;
	}

	R->ESI<int>(ChatModeToLegacyAscii(mode));

	return 0x55E4C9;
}
