#include <Helpers/Macro.h>
// in src/Misc/TextInput.cpp
#include <NetworkPackets.h>

namespace ChatScopeDisplay
{
	constexpr DWORD ScopeOffset = 0x118;
	constexpr DWORD MarkerOffset = 0x119;
	constexpr BYTE MarkerValue = 0xC7;

	constexpr auto SenderNameAddr = 0x00A8D668;
	constexpr auto SenderNameCap =
		(0x00A8D748 - 0x00A8D668) / sizeof(wchar_t); // 112 wchar_t

	ChatScope NormalizeMode(int mode)
	{
		return (mode == static_cast<int>(ChatScope::Team))
			? ChatScope::Team : ChatScope::All;
	}

	void StampScopeToPacket()
	{
		auto scope = NormalizeMode(*reinterpret_cast<int*>(CHAT_MODE_VARIABLE));
		*reinterpret_cast<BYTE*>(CHAT_PACKET_BUFFER + ScopeOffset) = static_cast<BYTE>(scope);
		*reinterpret_cast<BYTE*>(CHAT_PACKET_BUFFER + MarkerOffset) = MarkerValue;
	}

	ChatScope ScopeFromPacket()
	{
		if (*reinterpret_cast<BYTE*>(CHAT_PACKET_BUFFER + MarkerOffset) != MarkerValue)
		{
			return ChatScope::All;
		}
		auto raw = *reinterpret_cast<BYTE*>(CHAT_PACKET_BUFFER + ScopeOffset);
		return (raw == static_cast<BYTE>(ChatScope::Team)) ? ChatScope::Team : ChatScope::All;
	}

	void ApplyPrefix(ChatScope scope)
	{
		auto* name = reinterpret_cast<wchar_t*>(SenderNameAddr);
		if (!name[0]) { return; }

		const wchar_t* tag = (scope == ChatScope::Team) ? L"Team" : L"All";
		wchar_t tmp[SenderNameCap] = {};
		_snwprintf_s(tmp, _countof(tmp), _TRUNCATE, L"[%ls] %ls", tag, name);
		wcsncpy_s(name, SenderNameCap, tmp, _TRUNCATE);
	}
}

// 0x55EE14: preserve original instruction + stamp scope
DEFINE_HOOK(0x55EE14, MessageInput_Write_StampScope, 0x5)
{
	*reinterpret_cast<DWORD*>(0x00A8D74C) = R->EAX<DWORD>(); // original MOV
	ChatScopeDisplay::StampScopeToPacket();
	return 0x55EE19;
}

// 0x55F0E3: local print prefix
DEFINE_HOOK(0x55F0E3, MessageInput_Print_PrefixScope, 0x5)
{
	ChatScopeDisplay::ApplyPrefix(
		ChatScopeDisplay::NormalizeMode(*reinterpret_cast<int*>(CHAT_MODE_VARIABLE)));
	R->ECX(0x00A8BC60); // original MOV ECX,0xA8BC60
	return 0x55F0E8;
}

// 0x48D96C: incoming print prefix
DEFINE_HOOK(0x48D96C, NetMessage_Print_PrefixScope, 0x5)
{
	ChatScopeDisplay::ApplyPrefix(ChatScopeDisplay::ScopeFromPacket());
	R->ECX(0x00A8BC60); // original MOV ECX,0xA8BC60
	return 0x48D971;
}

// Allow message entry in Skirmish
// DEFINE_JUMP(LJMP, 0x55E484, 0x55E48D);

wchar_t* IMEBuffer = reinterpret_cast<wchar_t*>(0xB730EC);

UINT GetCurentCodepage()
{
	char szLCData[6 + 1];
	WORD lang = LOWORD(GetKeyboardLayout(NULL));
	LCID locale = MAKELCID(lang, SORT_DEFAULT);
	GetLocaleInfoA(locale, LOCALE_IDEFAULTANSICODEPAGE, szLCData, _countof(szLCData));

	return atoi(szLCData);
}

wchar_t LocalizeCharacter(char character)
{
	wchar_t result;
	UINT codepage = GetCurentCodepage();
	MultiByteToWideChar(codepage, MB_USEGLYPHCHARS, &character, 1, &result, 1);
	return result;
}

DEFINE_HOOK(0x5D46C7, MessageListClass_Input, 0x5)
{
	if (!IMEBuffer[0])
		R->EBX<wchar_t>(LocalizeCharacter(R->EBX<char>()));

	return 0;
}

DEFINE_HOOK(0x61510E, WWUI_NewEditCtrl, 0x7)
{
	R->EDI<wchar_t>(LocalizeCharacter(R->EBX<char>()));
	return 0x615226;
}

// It is required to add Imm32.lib to AdditionalDependencies
/*
HIMC& IMEContext = *reinterpret_cast<HIMC*>(0xB7355C);
wchar_t* IMECompositionString = reinterpret_cast<wchar_t*>(0xB73318);

DEFINE_HOOK(0x777F15, IMEUpdateCompositionString, 0x7)
{
	IMECompositionString[0] = 0;
	ImmGetCompositionStringW(IMEContext, GCS_COMPSTR, IMECompositionString, 256);

	return 0;
}
*/
