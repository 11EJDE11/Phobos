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

#include <CCINIClass.h>
#include <InputManagerClass.h>
#include <RulesClass.h>
#include <Surface.h>
#include <TacticalClass.h>
#include <WWMouseClass.h>

#include <Ext/Sidebar/SWSidebar/SWSidebarClass.h>
#include <Misc/MessageColumn.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr double DefaultZoomFactor = 1.0;
	constexpr int MouseWheelZoomSteps = 5;
	bool ConsumeSidebarWheelScroll = false;

	bool IsCursorInTacticalView()
	{
		if (!WWMouseClass::Instance)
			return false;

		const auto& mousePosition = WWMouseClass::Instance->XY1;
		const auto& viewBounds = DSurface::ViewBounds;

		return mousePosition.X >= viewBounds.X
			&& mousePosition.X < (viewBounds.X + viewBounds.Width)
			&& mousePosition.Y >= viewBounds.Y
			&& mousePosition.Y < (viewBounds.Y + viewBounds.Height);
	}

	bool SetMapZoomFactor(double factor)
	{
		if (!TacticalClass::Instance || factor <= 0.0)
			return false;

		if (std::abs(TacticalClass::Instance->ZoomInFactor - factor) < 0.0001)
			return true;

		TacticalClass::Instance->ZoomInFactor = factor;

		if (InputManagerClass::Instance)
			InputManagerClass::Instance->DoSomething();

		return true;
	}

	double GetSteppedZoomFactor(double currentZoomFactor, double targetZoomFactor, bool zoomIn)
	{
		const auto delta = targetZoomFactor - DefaultZoomFactor;
		if (std::abs(delta) < 0.0001 || MouseWheelZoomSteps <= 0)
			return zoomIn ? targetZoomFactor : DefaultZoomFactor;

		if (!std::isfinite(currentZoomFactor) || currentZoomFactor <= 0.0)
			currentZoomFactor = DefaultZoomFactor;

		const auto minZoom = std::min(DefaultZoomFactor, targetZoomFactor);
		const auto maxZoom = std::max(DefaultZoomFactor, targetZoomFactor);
		currentZoomFactor = std::clamp(currentZoomFactor, minZoom, maxZoom);

		const auto progress = (currentZoomFactor - DefaultZoomFactor) / delta;
		auto stepIndex = static_cast<int>(std::lround(std::clamp(progress, 0.0, 1.0) * MouseWheelZoomSteps));

		stepIndex += zoomIn ? 1 : -1;
		stepIndex = std::clamp(stepIndex, 0, MouseWheelZoomSteps);

		const auto steppedProgress = static_cast<double>(stepIndex) / MouseWheelZoomSteps;
		return DefaultZoomFactor + delta * steppedProgress;
	}

	bool TryMouseWheelMapZoom(bool zoomIn)
	{
		if (!RulesClass::Instance || !TacticalClass::Instance || !IsCursorInTacticalView())
			return false;

		const auto zoomInFactor = RulesClass::Instance->ZoomInFactor;
		if (zoomInFactor <= 0.0)
			return false;

		const auto nextZoom = GetSteppedZoomFactor(TacticalClass::Instance->ZoomInFactor, zoomInFactor, zoomIn);
		return SetMapZoomFactor(nextZoom);
	}
}

DEFINE_HOOK(0x533066, CommandClassCallback_Register, 0x6)
{
	// Load it after Ares'

	MakeCommand<NextIdleHarvesterCommandClass>();
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
	{
		MessageColumnClass::Instance.ScrollDown();
		ConsumeSidebarWheelScroll = true;
		return;
	}

	ConsumeSidebarWheelScroll = TryMouseWheelMapZoom(false);
}

static void MouseWheelUpCommand()
{
	if (MessageColumnClass::Instance.IsHovering())
	{
		MessageColumnClass::Instance.ScrollUp();
		ConsumeSidebarWheelScroll = true;
		return;
	}

	ConsumeSidebarWheelScroll = TryMouseWheelMapZoom(true);
}

DEFINE_HOOK(0x777998, Game_WndProc_ScrollMouseWheel, 0x6)
{
	GET(const WPARAM, WParam, ECX);
	ConsumeSidebarWheelScroll = false;

	if (WParam & 0x80000000u)
		MouseWheelDownCommand();
	else
		MouseWheelUpCommand();

	return 0;
}

static inline bool CheckSkipScrollSidebar()
{
	if (ConsumeSidebarWheelScroll)
	{
		ConsumeSidebarWheelScroll = false;
		return true;
	}

	return MessageColumnClass::Instance.IsHovering();
}

DEFINE_HOOK(0x533F50, Game_ScrollSidebar_Skip, 0x5)
{
	enum { SkipScrollSidebar = 0x533FC3 };
	return CheckSkipScrollSidebar() ? SkipScrollSidebar : 0;
}
