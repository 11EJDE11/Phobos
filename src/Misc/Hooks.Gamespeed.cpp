
// The game normally limits FPS based on the GameSpeed index (0-6):
//   -GameSpeed 0 (fastest) = 60 FPS in multiplayer, configurable in skirmish
//   -GameSpeed 1 = 45 FPS
//   -GameSpeed 2+ = 60 / GameSpeed (e.g., 2=30fps, 3=20fps, etc.)
//
// This implementation provides custom FPS control by:
//   1) Hooking Queue_AI_Multiplayer (0x647C28, 0x647C4D) to remove the 60 FPS cap
//   2) Hooking MainLoop (0x55D7BC) to set correct frame timing for skirmish/campaign
//   3) Hooking SyncDelay (0x55E160, 0x55E33B) to maintain custom frame timing
//
// Add to rulesmd.ini under [General]:
//   EnableCustomFPS=yes          ; Enable/disable custom FPS (default: yes) --Todo:change default to no
//   CustomGameSpeedFPS=120       ; Target FPS (default: 120, range: 30-240)  --Todo: Update default
//
// How it works:
//   -When GameSpeed is set to 0 (fastest), the game will run at CustomGameSpeedFPS
//   -Other GameSpeed values use vanilla calculations
//   -Works in both skirmish and multiplayer modes
//
// Memory addresses:
//   - NFTTimer: 0x887328 (multiplayer frame timer)
//   - FrameTimer: 0x887348 (skirmish/campaign frame timer)
// =============================================================================

#include <Phobos.h>
#include <Utilities/Macro.h>
#include <SessionClass.h>
#include <GameOptionsClass.h>
#include <Unsorted.h>

#include <algorithm>

DEFINE_HOOK(0x69BAE7, SessionClass_Resume_CampaignGameSpeed, 0xA)
{
	GameOptionsClass::Instance.GameSpeed = Phobos::Config::CampaignDefaultGameSpeed;
	return 0x69BAF1;
}

// =============================================================================
// Multiplayer
// =============================================================================
// Hook 1: Remove the initial 60 FPS cap (was: mov esi, 0x3C)
// This sets the maximum allowed FPS before game speed calculations
DEFINE_HOOK(0x647C28, Queue_AI_Multiplayer_RemoveFPSCap, 0x5)
{
	if (!Phobos::Misc::EnableCustomFPS)
		return 0;

	// ESI is the cap value (was hardcoded to 60)
	// Set it to our target FPS instead
	R->ESI(Phobos::Misc::CustomGameSpeedFPS);
	return 0x647C2D; // Skip the original "mov esi, 0x3C"
}

// Hook 2: Override GameSpeed-to-FPS conversion for multiplayer
// The game runs this:
/*
	LABEL_73:
	v26 = 60;
	LABEL_76:
	v68 = v26;
	if (Options.GameSpeed)
	{
		if (Options.GameSpeed == 1)
		{
			v28 = 45;
		}
		else
		{
			v28 = 60 / Options.GameSpeed;
		}
	}
	else
	{
		v28 = 60;
	}
	if (v26 >= v28)
	{
		v68 = v28;
	}
*/
// ...which means we can't go higher than 60fps. So we skip it and do it ourselves.
// 1) First check if we've enabled a custom speed and the GameSpeed is 0
//		If so, overwrite EAX with the new speed. <---------note: perhaps it's OK already based on the earlier hook? Debug and check.
//		We check GameSpeed is 0 so the player can still change the speed
//		throughout the game. There's an issue with this where if the custom
//		FPS is less than 60, then the speed slider doesn't make sense
//			10 > 20 > 30 > 45 > 60 > 35
// 2) Then skip the final check that locks us to 60fps: if (v26 >= v28)
DEFINE_HOOK(0x647C4D, Queue_AI_Multiplayer_CustomFPSCalculation, 0x1F)
{
	int gameSpeed = GameOptionsClass::Instance.GameSpeed;
	int calculatedFPS;

	if (Phobos::Misc::EnableCustomFPS && gameSpeed == 0)
	{
		// Custom FPS when GameSpeed is 0 (fastest)
		calculatedFPS = Phobos::Misc::CustomGameSpeedFPS;
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



// =============================================================================
// Campaign/Skirmish/Multiplayer with ProtocolVersion != 2
// =============================================================================

// NFTTimer structure
struct NFTTimerStruct
{
	int StartTime;
	int CurrentTime_dummy;
	int TimeLeft;
};
DEFINE_REFERENCE(NFTTimerStruct, NFTTimer, 0x887328);

// Hook MainLoop skirmish/campaign FPS calculation
// This runs for GAME_CAMPAIGN (0), GAME_SKIRMISH (5), and multiplayer with ProtocolVersion != 2
// Location: Where FrameTimer is set up from GameSpeed value
// We need to set up BOTH FrameTimer and NFTTimer like multiplayer mode does for >60 FPS support     --Todo: double check 
DEFINE_HOOK(0x55D7B6, MainLoop_SkirmishFPSFix, 0xC)
{
	const DWORD timerValue = R->ECX();
	const int gameSpeed = R->ESI(); // ESI contains GameSpeed

	const bool shouldUseCustomFPS = Phobos::Misc::EnableCustomFPS
		&& gameSpeed == 0
		&& SessionClass::Instance.GameMode == GameMode::Skirmish;

	if (!shouldUseCustomFPS)
	{
		// Use vanilla behavior
		*reinterpret_cast<DWORD*>(0x88734C) = timerValue;      // FrameTimer.Timer        ------------------Is this necessary? And line below. I think yes if we return to 0x55D7C2, but no if we can return 0
		*reinterpret_cast<int*>(0x887350) = gameSpeed;         // FrameTimer.DelayTime = GameSpeed
		return 0x55D7C2;
	}

	// Custom FPS mode: Calculate frame timings
	const int targetFrameDelayTicks = std::max(0, 60 / Phobos::Misc::CustomGameSpeedFPS);
	const int targetFrameTimeMs = 1000 / Phobos::Misc::CustomGameSpeedFPS;

	// Get current time for NFTTimer initialization
	const DWORD currentTime = timeGetTime();

	// Set up FrameTimer (tick-based timing) - original instructions we're replacing
	*reinterpret_cast<DWORD*>(0x88734C) = timerValue;            // FrameTimer.Timer (from timer init)
	*reinterpret_cast<int*>(0x887350) = targetFrameDelayTicks;   // FrameTimer.DelayTime (0 for >60 FPS)

	// Set up NFTTimer (millisecond-precision timing) - critical for >60 FPS support
	// This is what multiplayer mode sets up but skirmish doesn't!
	*reinterpret_cast<DWORD*>(0x887328) = currentTime;           // NFTTimer.Started      ------------------------hmmm...are we changing this every frame? Is that correct?
	*reinterpret_cast<DWORD*>(0x88732C) = currentTime;           // NFTTimer.Timer        ------------------------hmmm...are we changing this every frame? Is that correct?
	*reinterpret_cast<int*>(0x887330) = targetFrameTimeMs;       // NFTTimer.Accumulated (ms per frame)

	// Also update Session.DesiredFrameRate for consistency
	SessionClass::Instance.DesiredFrameRate = Phobos::Misc::CustomGameSpeedFPS;

	// Skip the two original instructions (MOV [0x88734C],ECX and MOV [0x887350],ESI)
	return 0x55D7C2;
}

// =============================================================================
// CRITICAL FIX: Redirect Skirmish to use NFTTimer instead of FrameTimer
// =============================================================================

// Hook the skirmish mode check in SyncDelay to redirect it to the NFTTimer path
// This is THE KEY to making >60 FPS work in skirmish!
// Original: JZ LAB_0055e2b4 (jumps to FrameTimer-only path if Session.Type == 5)
// Fixed: Explicitly control all paths to avoid hook conflicts
DEFINE_HOOK(0x55E1B6, SyncDelay_RedirectSkirmishToNFTTimer, 0x6)
{
	// EAX contains Session.Type at this point (just compared with 5)
	// Original instruction: JZ LAB_0055e2b4 (jump if Session.Type == 5, i.e., skirmish)

	const int sessionType = R->EAX();

	if (sessionType == 5) // GAME_SKIRMISH
	{
		// Only redirect to NFTTimer path if custom FPS is enabled AND GameSpeed is 0 (fastest)
		if (Phobos::Misc::EnableCustomFPS && GameOptionsClass::Instance.GameSpeed == 0)		//-----------------------What will changing the game speed in-game do?
		{
			// Custom FPS enabled in skirmish: use NFTTimer path (like multiplayer)
			return 0x55E1BC;
		}
		else
		{
			// Custom FPS disabled or slow speed selected: use vanilla FrameTimer path
			return 0x55E2B4;
		}
	}

	return 0x55E1BC;
}
