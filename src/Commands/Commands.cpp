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
#include <vector>
#include <CCINIClass.h>
#include <Utilities/GeneralUtils.h>

#include <Ext/Techno/Body.h>
#include <Ext/Sidebar/SWSidebar/SWSidebarClass.h>
#include <Misc/MessageColumn.h>

namespace
{
	constexpr int InvalidFormationOffset = INT32_MIN;
	using FormationShapeType = TechnoExt::FormationShapeType;

	static bool IsFormationEligibleFoot(FootClass* const pFoot)
	{
		if (!pFoot)
			return false;

		auto const whatAmI = pFoot->WhatAmI();
		return whatAmI == AbstractType::Unit || whatAmI == AbstractType::Infantry;
	}

	// Assigns RA1-style mathematical offsets for a named formation shape to all group members.
	static void ApplyFormationShape(int team, HouseClass* pCurrentPlayer, FormationShapeType shape)
	{
		std::vector<FootClass*> members;
		int formationMaxSpeed = INT32_MAX;
		SpeedType formationSpeed = SpeedType::Wheel;

		for (auto const pFoot : FootClass::Array)
		{
			if (!pFoot || !IsFormationEligibleFoot(pFoot) || pFoot->InLimbo
				|| pFoot->Owner != pCurrentPlayer || pFoot->Group != team)
				continue;

			members.push_back(pFoot);

			if (auto const pType = pFoot->GetTechnoType())
			{
				if (pType->Speed < formationMaxSpeed)
				{
					formationMaxSpeed = pType->Speed;
					if (pFoot->WhatAmI() == AbstractType::Unit)
						formationSpeed = pType->SpeedType;
				}
			}
		}

		const int n = static_cast<int>(members.size());
		if (n == 0 || formationMaxSpeed == INT32_MAX)
			return;

		// Sort so the unit already closest to the tip direction gets the tip slot.
		switch (shape)
		{
		case FormationShapeType::LineEW:
		case FormationShapeType::WedgeW:
			std::sort(members.begin(), members.end(), [](FootClass* a, FootClass* b) {
				return a->GetMapCoords().X < b->GetMapCoords().X;
			});
			break;
		case FormationShapeType::WedgeE:
			std::sort(members.begin(), members.end(), [](FootClass* a, FootClass* b) {
				return a->GetMapCoords().X > b->GetMapCoords().X;
			});
			break;
		case FormationShapeType::LineNS:
		case FormationShapeType::WedgeN:
			std::sort(members.begin(), members.end(), [](FootClass* a, FootClass* b) {
				return a->GetMapCoords().Y < b->GetMapCoords().Y;
			});
			break;
		case FormationShapeType::WedgeS:
			std::sort(members.begin(), members.end(), [](FootClass* a, FootClass* b) {
				return a->GetMapCoords().Y > b->GetMapCoords().Y;
			});
			break;
		default:
			break;
		}

		// Calculate and assign RA1-faithful offsets, relative to the move destination.
		int xdir = 0, ydir = 0;
		bool evenodd = true;

		switch (shape)
		{
		case FormationShapeType::LineEW:
			xdir = -(n / 2);
			for (auto const pFoot : members)
			{
				auto const pExt = TechnoExt::ExtMap.Find(pFoot);
				pExt->FormationOffsetX = xdir;
				pExt->FormationOffsetY = 0;
				pExt->FormationOffsetValid = true;
				pExt->FormationShape = shape;
				pExt->FormationMoveSpeed = formationSpeed;
				pExt->FormationMoveMaxSpeed = formationMaxSpeed;
				xdir += 2;
			}
			break;

		case FormationShapeType::LineNS:
			ydir = -(n / 2);
			for (auto const pFoot : members)
			{
				auto const pExt = TechnoExt::ExtMap.Find(pFoot);
				pExt->FormationOffsetX = 0;
				pExt->FormationOffsetY = ydir;
				pExt->FormationOffsetValid = true;
				pExt->FormationShape = shape;
				pExt->FormationMoveSpeed = formationSpeed;
				pExt->FormationMoveMaxSpeed = formationMaxSpeed;
				ydir += 2;
			}
			break;

		case FormationShapeType::WedgeN:
		case FormationShapeType::WedgeS:
			{
				// WedgeN: tip at north (negative Y), units fan south (+yStep).
				// WedgeS: tip at south (positive Y), units fan north (-yStep).
				const int yStep = (shape == FormationShapeType::WedgeN) ? 2 : -2;
				ydir = (shape == FormationShapeType::WedgeN) ? -(n / 2) : (n / 2);
				xdir = 0;
				evenodd = true;
				for (auto const pFoot : members)
				{
					auto const pExt = TechnoExt::ExtMap.Find(pFoot);
					pExt->FormationOffsetX = xdir;
					pExt->FormationOffsetY = ydir;
					pExt->FormationOffsetValid = true;
					pExt->FormationShape = shape;
					pExt->FormationMoveSpeed = formationSpeed;
					pExt->FormationMoveMaxSpeed = formationMaxSpeed;
					xdir = -xdir;
					evenodd = !evenodd;
					if (!evenodd)
					{
						xdir -= 2;
						ydir += yStep;
					}
				}
			}
			break;

		case FormationShapeType::WedgeE:
		case FormationShapeType::WedgeW:
			{
				// WedgeE: tip at east (positive X), units fan west (-xStep).
				// WedgeW: tip at west (negative X), units fan east (+xStep).
				const int xStep = (shape == FormationShapeType::WedgeE) ? -2 : 2;
				xdir = (shape == FormationShapeType::WedgeE) ? (n / 2) : -(n / 2);
				ydir = 0;
				evenodd = true;
				for (auto const pFoot : members)
				{
					auto const pExt = TechnoExt::ExtMap.Find(pFoot);
					pExt->FormationOffsetX = xdir;
					pExt->FormationOffsetY = ydir;
					pExt->FormationOffsetValid = true;
					pExt->FormationShape = shape;
					pExt->FormationMoveSpeed = formationSpeed;
					pExt->FormationMoveMaxSpeed = formationMaxSpeed;
					ydir = -ydir;
					evenodd = !evenodd;
					if (!evenodd)
					{
						xdir += xStep;
						ydir -= 2;
					}
				}
			}
			break;

		default:
			break;
		}
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
					pExt->FormationShape = FormationShapeType::Custom;
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
				pExt->FormationShape = FormationShapeType::Custom;
			}
		}
	};

	class CycleFormationShapeCommandClass final : public CommandClass
	{
	public:
		virtual const char* GetName() const override
		{
			return "Cycle Formation Shape";
		}

		virtual const wchar_t* GetUIName() const override
		{
			return GeneralUtils::LoadStringUnlessMissing("TXT_CYCLE_FORMATION_SHAPE", L"Cycle Formation Shape");
		}

		virtual const wchar_t* GetUICategory() const override
		{
			return CATEGORY_SELECTION;
		}

		virtual const wchar_t* GetUIDescription() const override
		{
			return GeneralUtils::LoadStringUnlessMissing("TXT_CYCLE_FORMATION_SHAPE_DESC", L"Cycle through formation shapes for the selected control group.");
		}

		virtual void Execute(WWKey eInput) const override
		{
			UNREFERENCED_PARAMETER(eInput);

			auto const pCurrentPlayer = HouseClass::CurrentPlayer;

			if (!pCurrentPlayer)
				return;

			int team = -1;
			FormationShapeType currentShape = FormationShapeType::Custom;

			// Find the first selected foot unit that is in an active formation.
			for (auto const pFoot : FootClass::Array)
			{
				if (!pFoot || !IsFormationEligibleFoot(pFoot) || pFoot->InLimbo
					|| pFoot->Owner != pCurrentPlayer || !pFoot->IsSelected)
					continue;

				auto const pExt = TechnoExt::ExtMap.Find(pFoot);

				if (!pExt->FormationOffsetValid)
					return; // Not in a formation, nothing to cycle.

				team = pFoot->Group;
				currentShape = pExt->FormationShape;
				break;
			}

			if (team < 0)
				return;

			const int next = (static_cast<int>(currentShape) + 1) % static_cast<int>(FormationShapeType::Count);
			const auto nextShape = static_cast<FormationShapeType>(next);

			if (nextShape == FormationShapeType::Custom)
			{
				// Recalculate Custom shape from current unit positions.
				int minX = INT32_MAX, minY = INT32_MAX, maxX = INT32_MIN, maxY = INT32_MIN;
				int formationMaxSpeed = INT32_MAX;
				SpeedType formationSpeed = SpeedType::Wheel;

				for (auto const pFoot : FootClass::Array)
				{
					if (!pFoot || !IsFormationEligibleFoot(pFoot) || pFoot->InLimbo
						|| pFoot->Owner != pCurrentPlayer || pFoot->Group != team)
						continue;

					auto const mapCoords = pFoot->GetMapCoords();
					minX = std::min<int>(minX, mapCoords.X);
					maxX = std::max<int>(maxX, mapCoords.X);
					minY = std::min<int>(minY, mapCoords.Y);
					maxY = std::max<int>(maxY, mapCoords.Y);

					if (auto const pType = pFoot->GetTechnoType())
					{
						if (pType->Speed < formationMaxSpeed)
						{
							formationMaxSpeed = pType->Speed;
							if (pFoot->WhatAmI() == AbstractType::Unit)
								formationSpeed = pType->SpeedType;
						}
					}
				}

				if (formationMaxSpeed == INT32_MAX)
					return;

				const int centerX = minX + ((maxX - minX) / 2);
				const int centerY = minY + ((maxY - minY) / 2);

				for (auto const pFoot : FootClass::Array)
				{
					if (!pFoot || !IsFormationEligibleFoot(pFoot) || pFoot->InLimbo
						|| pFoot->Owner != pCurrentPlayer || pFoot->Group != team)
						continue;

					auto const pExt = TechnoExt::ExtMap.Find(pFoot);
					auto const mapCoords = pFoot->GetMapCoords();
					pExt->FormationOffsetValid = true;
					pExt->FormationOffsetX = static_cast<int>(mapCoords.X) - centerX;
					pExt->FormationOffsetY = static_cast<int>(mapCoords.Y) - centerY;
					pExt->FormationShape = FormationShapeType::Custom;
					pExt->FormationMoveSpeed = formationSpeed;
					pExt->FormationMoveMaxSpeed = formationMaxSpeed;
				}
			}
			else
			{
				ApplyFormationShape(team, pCurrentPlayer, nextShape);
			}
		}
	};
}

DEFINE_HOOK(0x533066, CommandClassCallback_Register, 0x6)
{
	// Load it after Ares'

	MakeCommand<NextIdleHarvesterCommandClass>();
	MakeCommand<ToggleFormationCommandClass>();
	MakeCommand<CycleFormationShapeCommandClass>();
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
