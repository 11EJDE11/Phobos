#pragma once

#include "Commands.h"

namespace ChatModeHotkeys
{
	inline constexpr const char* ChatToAllCommandName = "ChatToAll";
	inline constexpr const char* ChatToAlliesCommandName = "ChatToAllies";
	inline constexpr const char* PageUserCommandName = "PageUser";

	class ChatToAllCommandClass final : public CommandClass
	{
	public:
		virtual const char* GetName() const override;
		virtual const wchar_t* GetUIName() const override;
		virtual const wchar_t* GetUICategory() const override;
		virtual const wchar_t* GetUIDescription() const override;
		virtual void Execute(WWKey eInput) const override;
	};

	class ChatToAlliesCommandClass final : public CommandClass
	{
	public:
		virtual const char* GetName() const override;
		virtual const wchar_t* GetUIName() const override;
		virtual const wchar_t* GetUICategory() const override;
		virtual const wchar_t* GetUIDescription() const override;
		virtual void Execute(WWKey eInput) const override;
	};
}
