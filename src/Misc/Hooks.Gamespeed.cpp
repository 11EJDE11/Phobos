
// The game normally limits FPS based on the GameSpeed index (0-6):
//   -GameSpeed 0 (fastest) = 60 FPS in multiplayer, configurable in skirmish
//   -GameSpeed 1 = 45 FPS
//   -GameSpeed 2+ = 60 / GameSpeed (e.g., 2=30fps, 3=20fps, etc.)
//
// We add custom FPS control by:
//   1) Patching 0x647C28 and hooking 0x647C4D in Queue_AI_Multiplayer for FPS calculation
//   2) Hooking MainLoop (0x55D7B6) to set up dual timers for skirmish >60 FPS
//   3) Hooking SyncDelay (0x55E1B6) to redirect skirmish to the NFTTimer code path
//
// Add to rulesmd.ini under [General]:
//   EnableCustomFPS=yes              ; Enable/disable custom FPS (default: yes)
//   CustomGameSpeedFPS.0=120         ; Per-speed FPS. 0 = vanilla. Vanilla: 0=60, 1=45, 2=30, 3=20, 4=15, 5=12, 6=10
//
// How it works:
//   -Each speed slot with a non-zero CustomGameSpeedFPS.N runs at that target FPS
//   -Slots with 0 (or unset) use vanilla FPS for that position
//   -Practical max is ~1000 FPS (timeGetTime() resolution)
// =============================================================================

#include <Phobos.h>
#include <Utilities/Macro.h>
#include <algorithm>
#include <SessionClass.h>
#include <GameOptionsClass.h>
#include <Unsorted.h>

DEFINE_HOOK(0x69BAE7, SessionClass_Resume_CampaignGameSpeed, 0xA)
{
	GameOptionsClass::Instance.GameSpeed = Phobos::Config::CampaignDefaultGameSpeed;
	return 0x69BAF1;
}

// Patch v26 (ESI) to INT_MAX, disables the 60-FPS cap on the multiplayer FPS calculation.
// The hook below handles all conditional logic; cap check at 0x647C6C gives
// v68 = min(v26, v28), so v26 just needs to be >= max possible CustomGameSpeedFPS.
DEFINE_PATCH(0x647C28, 0xBE, 0xFF, 0xFF, 0xFF, 0x7F); // mov esi, INT_MAX

// Hook: Override the GameSpeed-to-v28 calculation in multiplayer.
// Original: v26 = 60, v68 = v26, calculate v28 from GameSpeed, then v68 = min(v26, v28).
// The patch at 0x647C28 sets v26 = INT_MAX, so the cap is effectively disabled.
// We set v28 (EAX) here; the cap check at 0x647C6C then gives v68 = v28.
DEFINE_HOOK(0x647C4D, Queue_AI_Multiplayer_CustomFPSCalculation, 0x1F)
{
	int gameSpeed = GameOptionsClass::Instance.GameSpeed;
	int calculatedFPS;

	if (Phobos::Misc::EnableCustomFPS && Phobos::Misc::CustomGameSpeedFPS[gameSpeed] > 0)
	{
		calculatedFPS = Phobos::Misc::CustomGameSpeedFPS[gameSpeed];
	}
	else if (gameSpeed == 0)
	{
		// Vanilla: GameSpeed 0 = 60 FPS
		calculatedFPS = 60;
	}
	else if (gameSpeed == 1)
	{
		// Vanilla: GameSpeed 1 = 45 FPS
		calculatedFPS = 45;
	}
	else
	{
		// Vanilla: GameSpeed 2+ = 60 / GameSpeed
		calculatedFPS = 60 / gameSpeed;
	}

	R->EAX(calculatedFPS);

	return 0x647C6C;
}

struct NFTTimerStruct
{
	DWORD StartTime;
	DWORD CurrentTime;
	int   TimeLeft;
};
DEFINE_REFERENCE(NFTTimerStruct, NFTTimer, 0x887328);

struct FrameTimerStruct
{
	DWORD StartTime;
	DWORD CurrentTime;
	int   DelayTime;
};
DEFINE_REFERENCE(FrameTimerStruct, GameFrameTimer, 0x887348);

// Hook MainLoop skirmish FPS calculation
// We need to set up both FrameTimer and NFTTimer like multiplayer mode does for >60 FPS support.
// FrameTimer.DelayTime rounds to 0 for >60 FPS (tick-based), so NFTTimer (ms-based)
// provides the actual frame timing that SyncDelay's NFTTimer loop uses.
DEFINE_HOOK(0x55D7B6, MainLoop_SkirmishFPSFix, 0xC)
{
	const DWORD timerValue = R->ECX();
	const int gameSpeed = R->ESI();

	const bool shouldUseCustomFPS = Phobos::Misc::EnableCustomFPS
		&& Phobos::Misc::CustomGameSpeedFPS[gameSpeed] > 0
		&& SessionClass::IsSkirmish();

	if (!shouldUseCustomFPS)
	{
		// Use vanilla behavior
		GameFrameTimer.CurrentTime = timerValue;
		GameFrameTimer.DelayTime = gameSpeed;
		return 0x55D7C2;
	}

	const int customFPS = Phobos::Misc::CustomGameSpeedFPS[gameSpeed];

	// calc frame timings
	const int targetFrameDelayTicks = 60 / customFPS;
	const int targetFrameTimeMs = std::max(1, 1000 / customFPS); // 1ms floor = ~1000 FPS ceiling

	const DWORD currentTime = timeGetTime();

	GameFrameTimer.CurrentTime = timerValue;
	GameFrameTimer.DelayTime = targetFrameDelayTicks;  // 0 for >60 FPS

	// SyncDelay compares elapsed time since CurrentTime against TimeLeft.
	NFTTimer.StartTime = currentTime;
	NFTTimer.CurrentTime = currentTime;
	NFTTimer.TimeLeft = targetFrameTimeMs;

	SessionClass::Instance.DesiredFrameRate = customFPS;

	return 0x55D7C2; // Past the two MOVs we replaced
}

// Hook the skirmish mode check in SyncDelay to redirect it to the NFTTimer path
// Original: JZ LAB_0055e2b4 (jumps to FrameTimer-only path if Session.Type == 5)
DEFINE_HOOK(0x55E1B6, SyncDelay_RedirectSkirmishToNFTTimer, 0x6)
{
	if (SessionClass::IsSkirmish())
	{
		if (Phobos::Misc::EnableCustomFPS && Phobos::Misc::CustomGameSpeedFPS[GameOptionsClass::Instance.GameSpeed] > 0)
			return 0x55E1BC; // Custom FPS: use NFTTimer path like multiplayer

		return 0x55E2B4; // Vanilla FrameTimer path
	}

	return 0x55E1BC;
}
