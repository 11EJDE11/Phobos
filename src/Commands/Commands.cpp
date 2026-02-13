#include "Commands.h"

#include "ObjectInfo.h"
#include "NextIdleHarvester.h"
#include "QuickSave.h"
#include "DamageDisplay.h"
#include "FrameByFrame.h"
#include "FrameStep.h"
#include "ToggleDigitalDisplay.h"
#include "ToggleDesignatorRange.h"
#include "SaveVariablesToFile.h"
#include "ToggleSWSidebar.h"
#include "FireTacticalSW.h"
#include "ToggleMessageList.h"

#include <algorithm>
#include <CCINIClass.h>
#include <Utilities/GeneralUtils.h>

#include <Ext/Techno/Body.h>
#include <Ext/Sidebar/SWSidebar/SWSidebarClass.h>
#include <Misc/MessageColumn.h>

namespace
{
	constexpr int InvalidFormationOffset = INT32_MIN;
	static bool IsFormationEligibleFoot(FootClass* const pFoot)
	{
		if (!pFoot)
			return false;

		auto const whatAmI = pFoot->WhatAmI();
		return whatAmI == AbstractType::Unit || whatAmI == AbstractType::Infantry;
	}

	class ToggleFormationCommandClass final : public CommandClass
	{
	public:
		virtual const char* GetName() const override
		{
			return "Toggle Formation";
		}

		virtual const wchar_t* GetUIName() const override
		{
			return GeneralUtils::LoadStringUnlessMissing("TXT_TOGGLE_FORMATION", L"Toggle Formation");
		}

		virtual const wchar_t* GetUICategory() const override
		{
			return CATEGORY_SELECTION;
		}

		virtual const wchar_t* GetUIDescription() const override
		{
			return GeneralUtils::LoadStringUnlessMissing("TXT_TOGGLE_FORMATION_DESC", L"Toggle formation offsets for selected control group.");
		}

		virtual void Execute(WWKey eInput) const override
		{
			UNREFERENCED_PARAMETER(eInput);

			auto const pCurrentPlayer = HouseClass::CurrentPlayer;

			if (!pCurrentPlayer)
				return;

			int team = -1;
			bool setFormation = false;
			int minX = INT32_MAX;
			int minY = INT32_MAX;
			int maxX = INT32_MIN;
			int maxY = INT32_MIN;
			int formationMaxSpeed = INT32_MAX;
			SpeedType formationSpeed = SpeedType::Wheel;

			for (auto const pFoot : FootClass::Array)
			{
				if (!pFoot || !IsFormationEligibleFoot(pFoot) || pFoot->InLimbo || pFoot->Owner != pCurrentPlayer || !pFoot->IsSelected)
					continue;

				team = pFoot->Group;

				if (team >= 0)
				{
					auto const pExt = TechnoExt::ExtMap.Find(pFoot);
					setFormation = !pExt->FormationOffsetValid;
				}

				break;
			}

			if (team < 0)
				return;

			for (auto const pFoot : FootClass::Array)
			{
				if (!pFoot || !IsFormationEligibleFoot(pFoot) || pFoot->InLimbo || pFoot->Owner != pCurrentPlayer || pFoot->Group != team)
					continue;

				auto const pExt = TechnoExt::ExtMap.Find(pFoot);
				pExt->FormationMoveActive = false;

				if (!setFormation)
				{
					pExt->FormationOffsetValid = false;
					pExt->FormationOffsetX = InvalidFormationOffset;
					pExt->FormationOffsetY = InvalidFormationOffset;
					pExt->FormationMoveSpeed = SpeedType::None;
					pExt->FormationMoveMaxSpeed = -1;
					continue;
				}

				auto const mapCoords = pFoot->GetMapCoords();
				minX = std::min<int>(minX, mapCoords.X);
				maxX = std::max<int>(maxX, mapCoords.X);
				minY = std::min<int>(minY, mapCoords.Y);
				maxY = std::max<int>(maxY, mapCoords.Y);

				auto const pType = pFoot->GetTechnoType();

				if (pType && pType->Speed < formationMaxSpeed)
				{
					formationMaxSpeed = pType->Speed;

					// RA1 formation speed type tracks vehicle locomotion; infantry-only keeps wheel default.
					if (pFoot->WhatAmI() == AbstractType::Unit)
						formationSpeed = pType->SpeedType;
				}
			}

			if (!setFormation || formationMaxSpeed == INT32_MAX)
				return;

			const int centerX = minX + ((maxX - minX) / 2);
			const int centerY = minY + ((maxY - minY) / 2);

			for (auto const pFoot : FootClass::Array)
			{
				if (!pFoot || !IsFormationEligibleFoot(pFoot) || pFoot->InLimbo || pFoot->Owner != pCurrentPlayer || pFoot->Group != team)
					continue;

				auto const pExt = TechnoExt::ExtMap.Find(pFoot);
				auto const mapCoords = pFoot->GetMapCoords();
				pExt->FormationOffsetValid = true;
				pExt->FormationOffsetX = static_cast<int>(mapCoords.X) - centerX;
				pExt->FormationOffsetY = static_cast<int>(mapCoords.Y) - centerY;
				pExt->FormationMoveSpeed = formationSpeed;
				pExt->FormationMoveMaxSpeed = formationMaxSpeed;
			}
		}
	};
}

DEFINE_HOOK(0x533066, CommandClassCallback_Register, 0x6)
{
	// Load it after Ares'

	MakeCommand<NextIdleHarvesterCommandClass>();
	MakeCommand<ToggleFormationCommandClass>();
	MakeCommand<QuickSaveCommandClass>();
	MakeCommand<ToggleDigitalDisplayCommandClass>();
	MakeCommand<ToggleDesignatorRangeCommandClass>();
	MakeCommand<ToggleMessageListCommandClass>();
	MakeCommand<ToggleSWSidebar>();

	if (Phobos::Config::SuperWeaponSidebarCommands)
	{
		SWSidebarClass::Commands[0] = MakeCommand<FireTacticalSWCommandClass<0>>();
		SWSidebarClass::Commands[1] = MakeCommand<FireTacticalSWCommandClass<1>>();
		SWSidebarClass::Commands[2] = MakeCommand<FireTacticalSWCommandClass<2>>();
		SWSidebarClass::Commands[3] = MakeCommand<FireTacticalSWCommandClass<3>>();
		SWSidebarClass::Commands[4] = MakeCommand<FireTacticalSWCommandClass<4>>();
		SWSidebarClass::Commands[5] = MakeCommand<FireTacticalSWCommandClass<5>>();
		SWSidebarClass::Commands[6] = MakeCommand<FireTacticalSWCommandClass<6>>();
		SWSidebarClass::Commands[7] = MakeCommand<FireTacticalSWCommandClass<7>>();
		SWSidebarClass::Commands[8] = MakeCommand<FireTacticalSWCommandClass<8>>();
		SWSidebarClass::Commands[9] = MakeCommand<FireTacticalSWCommandClass<9>>();
	}

	if (Phobos::Config::DevelopmentCommands)
	{
		MakeCommand<DamageDisplayCommandClass>();
		MakeCommand<SaveVariablesToFileCommandClass>();
		MakeCommand<ObjectInfoCommandClass>();
		MakeCommand<FrameByFrameCommandClass>();
		MakeCommand<FrameStepCommandClass<1>>(); // Single step in
		MakeCommand<FrameStepCommandClass<5>>(); // Speed 1
		MakeCommand<FrameStepCommandClass<10>>(); // Speed 2
		MakeCommand<FrameStepCommandClass<15>>(); // Speed 3
		MakeCommand<FrameStepCommandClass<30>>(); // Speed 4
		MakeCommand<FrameStepCommandClass<60>>(); // Speed 5
	}

	return 0;
}

static void MouseWheelDownCommand()
{
	if (MessageColumnClass::Instance.IsHovering())
		MessageColumnClass::Instance.ScrollDown();
}

static void MouseWheelUpCommand()
{
	if (MessageColumnClass::Instance.IsHovering())
		MessageColumnClass::Instance.ScrollUp();
}

DEFINE_HOOK(0x777998, Game_WndProc_ScrollMouseWheel, 0x6)
{
	GET(const WPARAM, WParam, ECX);

	if (WParam & 0x80000000u)
		MouseWheelDownCommand();
	else
		MouseWheelUpCommand();

	return 0;
}

static inline bool CheckSkipScrollSidebar()
{
	return MessageColumnClass::Instance.IsHovering();
}

DEFINE_HOOK(0x533F50, Game_ScrollSidebar_Skip, 0x5)
{
	enum { SkipScrollSidebar = 0x533FC3 };
	return CheckSkipScrollSidebar() ? SkipScrollSidebar : 0;
}
