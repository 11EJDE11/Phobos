#pragma once

#include <GeneralDefinitions.h>
#include <cstddef>

// Chat message packet scope/recipient
enum class ChatScope : BYTE
{
	Unknown = 0,
	All = 1,      // Enter key - send to all players
	Team = 2,     // Backspace key - send to allied players only
	Other = 3     // Backslash key - purpose unknown
};

#pragma pack(push, 1)

// Chat message packet structure (packet type 0x0C)
// Total size: 0x1C7 (455 bytes)
// Used in multiplayer to send chat messages between players
// Notes:
// - The vanilla game expects the chat text at offset 0x004.
// - Phobos may extend the packet using unused tail bytes.
struct ChatPacket
{
	DWORD PacketType;           // 0x000: 0x0C for chat messages
	char MessageText[268];      // 0x004-0x10F: Chat message text (null-terminated)
	DWORD PlayerColorID;        // 0x110: Sender's player color/ID
	DWORD ValidationID;         // 0x114: Validation value
	BYTE ExtScope;              // 0x118: Phobos extension (ChatScope)
	BYTE ExtMarker;             // 0x119: Phobos extension marker
	BYTE Unknown_11A[173];      // 0x11A-0x1C6: Unused/padding
};
static_assert(sizeof(ChatPacket) == 0x1C7, "ChatPacket size must be 0x1C7 (455 bytes)");
static_assert(offsetof(ChatPacket, MessageText) == 0x004);
static_assert(offsetof(ChatPacket, PlayerColorID) == 0x110);
static_assert(offsetof(ChatPacket, ValidationID) == 0x114);
static_assert(offsetof(ChatPacket, ExtScope) == 0x118);
static_assert(offsetof(ChatPacket, ExtMarker) == 0x119);
static_assert(offsetof(ChatPacket, Unknown_11A) == 0x11A);

#pragma pack(pop)

// Global chat packet buffer
constexpr auto CHAT_PACKET_BUFFER = 0x00A8D638;
constexpr auto CHAT_PACKET_TYPE = 0x0C;
constexpr auto CHAT_MODE_VARIABLE = 0x00ABCE18; // DAT_00abce18 - current chat mode

// Phobos chat extension offsets and marker.
constexpr auto CHAT_PACKET_EXT_SCOPE_OFFSET = 0x118;
constexpr auto CHAT_PACKET_EXT_MARKER_OFFSET = 0x119;
constexpr BYTE CHAT_PACKET_EXT_MARKER_VALUE = 0xC7;
