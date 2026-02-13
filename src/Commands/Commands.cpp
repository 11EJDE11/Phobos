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
#include <MessageListClass.h>

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

	constexpr int FormationLineMaxLen = 20;

	// Assigns formation offsets using 1-cell spacing, wrapping into multiple
	// rows/columns (lines) or a triangular fill (wedges) at FormationLineMaxLen.
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

		// Sort so the unit closest to the lead direction gets the tip/first slot.
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

		auto assign = [&](FootClass* pFoot, int xOff, int yOff)
		{
			auto const pExt = TechnoExt::ExtMap.Find(pFoot);
			pExt->FormationOffsetX = xOff;
			pExt->FormationOffsetY = yOff;
			pExt->FormationOffsetValid = true;
			pExt->FormationShape = shape;
			pExt->FormationMoveSpeed = formationSpeed;
			pExt->FormationMoveMaxSpeed = formationMaxSpeed;
		};

		switch (shape)
		{
		case FormationShapeType::LineEW:
			{
				// Rows of up to FormationLineMaxLen units wide, stacked along Y.
				const int W = FormationLineMaxLen;
				const int numRows = (n + W - 1) / W;
				int idx = 0;
				for (int row = 0; row < numRows && idx < n; row++)
				{
					const int rowSize = std::min(W, n - row * W);
					const int yOff = row - numRows / 2;
					const int xStart = -(rowSize / 2);
					for (int j = 0; j < rowSize; j++)
						assign(members[idx++], xStart + j, yOff);
				}
			}
			break;

		case FormationShapeType::LineNS:
			{
				// Columns of up to FormationLineMaxLen units tall, stacked along X.
				const int W = FormationLineMaxLen;
				const int numCols = (n + W - 1) / W;
				int idx = 0;
				for (int col = 0; col < numCols && idx < n; col++)
				{
					const int colSize = std::min(W, n - col * W);
					const int xOff = col - numCols / 2;
					const int yStart = -(colSize / 2);
					for (int j = 0; j < colSize; j++)
						assign(members[idx++], xOff, yStart + j);
				}
			}
			break;

		case FormationShapeType::WedgeN:
		case FormationShapeType::WedgeS:
		case FormationShapeType::WedgeE:
		case FormationShapeType::WedgeW:
			{
				// Stack multiple V-chevrons behind each other for a ">>>" appearance.
				// Each V holds up to (1+2*W) units. V's are separated by a 1-cell gap.
				// WedgeN/S orient along Y; WedgeE/W orient along X.
				const int W = FormationLineMaxLen / 2;
				const int vCapacity = 1 + 2 * W;
				const int numVs = (n + vCapacity - 1) / vCapacity;
				const int stride = W / 2;
				const int totalDepth = (numVs - 1) * stride + W + 1;
				const int tipY = -(totalDepth / 2);

				auto assignW = [&](FootClass* pFoot, int x, int y)
				{
					switch (shape)
					{
					case FormationShapeType::WedgeN: assign(pFoot, x, y); break;
					case FormationShapeType::WedgeS: assign(pFoot, x, -y); break;
					case FormationShapeType::WedgeE: assign(pFoot, -y, x); break;
					case FormationShapeType::WedgeW: assign(pFoot, y, x); break;
					default: break;
					}
				};

				// Place each V-chevron in sequence.
				int idx = 0;
				for (int v = 0; v < numVs && idx < n; v++)
				{
					const int vTipY = tipY + v * stride;
					const int vSize = std::min(vCapacity, n - idx);
					int xdir = 0, ydir = vTipY;
					bool evenodd = true;
					for (int k = 0; k < vSize; k++)
					{
						assignW(members[idx++], xdir, ydir);
						xdir = -xdir;
						evenodd = !evenodd;
						if (!evenodd)
						{
							xdir -= 1;
							ydir += 1;
						}
					}
				}
			}
			break;

		default:
			break;
		}
	}

	static const wchar_t* GetFormationShapeName(FormationShapeType shape)
	{
		switch (shape)
		{
		case FormationShapeType::Custom:  return L"Custom";
		case FormationShapeType::LineEW:  return L"Line (E/W)";
		case FormationShapeType::LineNS:  return L"Line (N/S)";
		case FormationShapeType::WedgeN:  return L"Wedge (North)";
		case FormationShapeType::WedgeE:  return L"Wedge (East)";
		case FormationShapeType::WedgeS:  return L"Wedge (South)";
		case FormationShapeType::WedgeW:  return L"Wedge (West)";
		default:                          return L"Unknown";
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

			wchar_t msgBuf[64];
			swprintf_s(msgBuf, L"Formation: %s", GetFormationShapeName(nextShape));
			MessageListClass::Instance.PrintMessage(
				msgBuf, RulesClass::Instance->MessageDelay,
				HouseClass::CurrentPlayer->ColorSchemeIndex, true);
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
