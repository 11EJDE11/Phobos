#include <PreviewClass.h>
#include <ThemeClass.h>
#include <FPSCounter.h>
#include <Fundamentals.h>
#include <IPXManagerClass.h>

#include <algorithm>
#include <climits>
#include <cwchar>

#include <Ext/House/Body.h>
#include <Ext/Side/Body.h>
#include <Ext/Scenario/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/SWType/Body.h>

#include <Misc/FlyingStrings.h>

#include <New/Entity/BannerClass.h>

#include <Utilities/Debug.h>

namespace
{
	constexpr int MPDebugPanelMargin = 8;
	constexpr int MPDebugPanelMinWidth = 340;
	constexpr int MPDebugPanelMaxWidth = 620;
	constexpr int MPDebugTableMaxRows = 8;
	constexpr int MPDebugHeaderHeight = 14;
	constexpr int MPDebugSummaryLineHeight = 12;
	constexpr int MPDebugTableHeaderHeight = 12;
	constexpr int MPDebugTableRowHeight = 12;

	struct MPDebugRow
	{
		wchar_t Name[64];
		int Process;
		int AverageRoundTrip;
		int MaxRoundTrip;
		int LossPercent;
		int Resends;
		int FrameSyncStalls;
		int CommandStalls;
		ColorStruct AccentColor;
	};

	struct MPDebugConnectionView
	{
		int Unknown_00;
		void* ConnectionState;
		int Resends;
		int Lost;
		int PercentLost;
		int FrameSyncStalls;
		int CommandStalls;
		char Unknown_1C[0x48];
		int HouseIndex;
		char NameRaw[64];
	};

	static int __fastcall GetConnectionAverageRoundTripFrames(void* const pConnectionState)
	{
		return pConnectionState
			? reinterpret_cast<int(__thiscall*)(void*)>(0x48BA80)(pConnectionState)
			: 0;
	}

	static int __fastcall GetConnectionMaxRoundTripFrames(void* const pConnectionState)
	{
		return pConnectionState
			? reinterpret_cast<int(__thiscall*)(void*)>(0x48BA90)(pConnectionState)
			: 0;
	}

	static int ClampToNonNegative(const int value)
	{
		return value > 0 ? value : 0;
	}

	static bool DecodeConnectionName(const MPDebugConnectionView* const pConn, wchar_t(&out)[64])
	{
		out[0] = L'\0';
		auto const* const bytes = reinterpret_cast<const unsigned char*>(pConn->NameRaw);
		if (!bytes[0])
			return false;

		// Some game paths store remote names as ANSI while others use UTF-16LE.
		// Detect UTF-16LE by checking that odd bytes are zero for at least 2 chars.
		int widePairs = 0;
		bool sawWideTerminator = false;
		for (int i = 0; i < 16; ++i)
		{
			const unsigned char lo = bytes[i * 2];
			const unsigned char hi = bytes[i * 2 + 1];
			if (!lo && !hi)
			{
				sawWideTerminator = true;
				break;
			}

			if (hi)
			{
				widePairs = 0;
				break;
			}

			++widePairs;
		}

		const bool isWideName = widePairs >= 2 || (widePairs == 1 && sawWideTerminator);
		if (isWideName)
			swprintf_s(out, L"%ls", reinterpret_cast<const wchar_t*>(pConn->NameRaw));
		else
			swprintf_s(out, L"%S", pConn->NameRaw);

		return out[0] != L'\0';
	}

	static int FindPlayerProcessByHouseIndex(const int houseIndex)
	{
		if (houseIndex < 0)
			return -1;

		for (int i = 0; i < NodeNameType::Array.Count; ++i)
		{
			if (auto const* const pNode = NodeNameType::Array.GetItem(i))
			{
				if (pNode->HouseIndex == houseIndex)
					return pNode->Time;
			}
		}

		return -1;
	}

	static int FindPlayerProcessByName(const wchar_t* const pName)
	{
		if (!pName || !pName[0])
			return -1;

		for (int i = 0; i < NodeNameType::Array.Count; ++i)
		{
			if (auto const* const pNode = NodeNameType::Array.GetItem(i))
			{
				if (pNode->Name[0] && !_wcsicmp(pNode->Name, pName))
					return pNode->Time;
			}
		}

		return -1;
	}

	static ColorStruct ResolvePlayerColorByName(const char* const pName)
	{
		if (!pName || !pName[0])
			return Drawing::TooltipColor;

		for (int i = 0; i < HouseClass::Array.Count; ++i)
		{
			if (const auto pHouse = HouseClass::Array.GetItem(i))
			{
				if (pHouse->PlainName[0] && !_stricmp(pHouse->PlainName, pName))
					return pHouse->Color;
			}
		}

		return Drawing::TooltipColor;
	}

	static int GatherMPDebugRowsFromConnections(MPDebugRow(&rows)[MPDebugTableMaxRows])
	{
		int count = 0;
		auto const& ipx = IPXManagerClass::Instance;
		const int activeConnections = std::clamp<int>(ipx.NumConnections, 0, 7);

		for (int i = 0; i < activeConnections && count < MPDebugTableMaxRows; ++i)
		{
			auto const* const pConn = reinterpret_cast<const MPDebugConnectionView*>(ipx.Connection[i]);
			if (!pConn)
				continue;

			if (!pConn->NameRaw[0])
				continue;

			auto const* const pHouse = HouseClass::Array.GetItemOrDefault(pConn->HouseIndex);
			if (!pHouse || pHouse == HouseClass::CurrentPlayer)
				continue;

			auto& row = rows[count++];
			if (!DecodeConnectionName(pConn, row.Name))
			{
				--count;
				continue;
			}
			row.Process = FindPlayerProcessByHouseIndex(pConn->HouseIndex);
			row.AverageRoundTrip = ClampToNonNegative((GetConnectionAverageRoundTripFrames(pConn->ConnectionState) * 1000) / 60);
			row.MaxRoundTrip = ClampToNonNegative((GetConnectionMaxRoundTripFrames(pConn->ConnectionState) * 1000) / 60);
			row.LossPercent = ClampToNonNegative(pConn->PercentLost);
			row.Resends = ClampToNonNegative(pConn->Resends);
			row.FrameSyncStalls = ClampToNonNegative(pConn->FrameSyncStalls);
			row.CommandStalls = ClampToNonNegative(pConn->CommandStalls);
			row.AccentColor = pHouse->Color;
		}

		return count;
	}

	static int GatherMPDebugRows(MPDebugRow(&rows)[MPDebugTableMaxRows])
	{
		int const fromConnections = GatherMPDebugRowsFromConnections(rows);
		if (fromConnections > 0)
			return fromConnections;

		int count = 0;
		auto const& session = SessionClass::Instance;

		for (int i = 0; i < 8 && count < MPDebugTableMaxRows; ++i)
		{
			auto const& stats = session.MPStats[i];
			if (!stats.Name[0])
				continue;

			auto& row = rows[count++];
			swprintf_s(row.Name, L"%S", stats.Name);
			row.Process = FindPlayerProcessByName(row.Name);
			row.AverageRoundTrip = ClampToNonNegative(stats.MaxAvgRoundTrip);
			row.MaxRoundTrip = ClampToNonNegative(stats.MaxRoundTrip);
			row.LossPercent = ClampToNonNegative(stats.PercentLost);
			row.Resends = ClampToNonNegative(stats.Resends);
			row.FrameSyncStalls = ClampToNonNegative(stats.FrameSyncStalls);
			row.CommandStalls = ClampToNonNegative(stats.CommandCoundStalls);
			row.AccentColor = ResolvePlayerColorByName(stats.Name);
		}

		return count;
	}

	static void DrawMPDebugText(
		const wchar_t* const pText,
		const RectangleStruct& clipRect,
		const int x,
		const int y,
		const ColorStruct& color,
		const TextPrintType flags = TextPrintType::NoShadow | TextPrintType::Point6
	)
	{
		Point2D drawPoint { x, y };
		RectangleStruct drawRect = clipRect;
		DSurface::Composite->DrawText(pText, &drawRect, &drawPoint, Drawing::RGB_To_Int(color), 0, flags);
	}

	static void DrawCompactMPDebugStats()
	{
		auto const pSurface = DSurface::Composite;
		if (!pSurface)
			return;

		RectangleStruct const viewBounds = DSurface::ViewBounds;
		const int availableWidth = std::max(240, viewBounds.Width - MPDebugPanelMargin * 2);
		const int panelWidth = std::max(MPDebugPanelMinWidth, std::min(MPDebugPanelMaxWidth, availableWidth));

		// Keep SessionClass::MPStats in sync in case another code path depends on it.
		if (SessionClass::IsMultiplayer())
			reinterpret_cast<void(__thiscall*)(IPXManagerClass*)>(0x542520)(&IPXManagerClass::Instance);

		MPDebugRow rows[MPDebugTableMaxRows] {};
		const int rowCount = GatherMPDebugRows(rows);
		const bool showTable = rowCount > 0;

		const int summaryHeight = MPDebugHeaderHeight + (2 * MPDebugSummaryLineHeight) + 8;
		const int tableHeight = showTable ? (MPDebugTableHeaderHeight + (rowCount * MPDebugTableRowHeight) + 8) : 14;
		const int panelHeight = summaryHeight + tableHeight + 8;

		RectangleStruct panelRect
		{
			viewBounds.X + MPDebugPanelMargin,
			viewBounds.Y + MPDebugPanelMargin,
			panelWidth,
			panelHeight
		};

		const ColorStruct panelColor { 12, 20, 30 };
		const ColorStruct borderColor { 58, 84, 112 };
		pSurface->FillRect(&panelRect, Drawing::RGB_To_Int(panelColor));
		pSurface->DrawRect(&panelRect, Drawing::RGB_To_Int(borderColor));

		const int textLeft = panelRect.X + 8;
		int textY = panelRect.Y + 5;

		const ColorStruct titleColor { 210, 230, 255 };
		DrawMPDebugText(
			L"MP Debug Stats",
			panelRect,
			textLeft,
			textY,
			titleColor,
			TextPrintType::NoShadow | TextPrintType::Point8
		);
		textY += MPDebugHeaderHeight;

		const wchar_t* pMode = SessionClass::IsCampaign()
			? L"Campaign"
			: SessionClass::IsSkirmish()
			? L"Skirmish"
			: SessionClass::IsMultiplayer()
			? L"Multiplayer"
			: L"Unknown";

		const int responseTime = (IPXManagerClass::Instance.ResponseTime() * 1000) / 60;
		wchar_t summaryLine[256];
		swprintf_s(
			summaryLine,
			L"Frame %d  FPS %u  Req %d  MaxAhead %d  Resp %dms",
			Unsorted::CurrentFrame,
			FPSCounter::CurrentFrameRate,
			Game::Network.RequestedFPS,
			Game::Network.MaxAhead,
			responseTime
		);
		DrawMPDebugText(summaryLine, panelRect, textLeft, textY, ColorStruct { 222, 232, 242 });
		textY += MPDebugSummaryLineHeight;

		swprintf_s(
			summaryLine,
			L"%ls  Players %d  Proc %d/%d  LatFudge %d",
			pMode,
			std::max(SessionClass::Instance.MPlayerCount, rowCount),
			SessionClass::Instance.ProcessTicks,
			SessionClass::Instance.ProcessFrames,
			Game::Network.LatencyFudge
		);
		DrawMPDebugText(summaryLine, panelRect, textLeft, textY, ColorStruct { 186, 204, 224 });
		textY += MPDebugSummaryLineHeight + 4;

		if (!showTable)
		{
			DrawMPDebugText(
				L"Waiting for remote player stats...",
				panelRect,
				textLeft,
				textY,
				ColorStruct { 156, 176, 198 }
			);
			return;
		}

		const int contentWidth = panelRect.Width - 16;
		const int colPlayer = textLeft;
		const int colProc = textLeft + (contentWidth * 37) / 100;
		const int colRtt = textLeft + (contentWidth * 48) / 100;
		const int colLoss = textLeft + (contentWidth * 66) / 100;
		const int colStalls = textLeft + (contentWidth * 84) / 100;

		DrawMPDebugText(L"Player", panelRect, colPlayer, textY, ColorStruct { 170, 200, 236 });
		DrawMPDebugText(L"Proc", panelRect, colProc, textY, ColorStruct { 170, 200, 236 });
		DrawMPDebugText(L"RTT avg/max", panelRect, colRtt, textY, ColorStruct { 170, 200, 236 });
		DrawMPDebugText(L"Loss/Resend", panelRect, colLoss, textY, ColorStruct { 170, 200, 236 });
		DrawMPDebugText(L"Stalls", panelRect, colStalls, textY, ColorStruct { 170, 200, 236 });
		textY += MPDebugTableHeaderHeight;

		int laggingIndex = -1;
		int lowestProcess = INT_MAX;
		for (int i = 0; i < rowCount; ++i)
		{
			if (rows[i].Process >= 0 && rows[i].Process < lowestProcess)
			{
				lowestProcess = rows[i].Process;
				laggingIndex = i;
			}
		}

		// Fallback when process info is not available.
		if (laggingIndex < 0)
		{
			laggingIndex = 0;
		}

		for (int i = 1; i < rowCount; ++i)
		{
			if (rows[i].AverageRoundTrip > rows[laggingIndex].AverageRoundTrip
				|| (rows[i].AverageRoundTrip == rows[laggingIndex].AverageRoundTrip
					&& rows[i].MaxRoundTrip > rows[laggingIndex].MaxRoundTrip))
			{
				if (lowestProcess == INT_MAX)
					laggingIndex = i;
			}
		}

		for (int i = 0; i < rowCount; ++i)
		{
			const ColorStruct lineColor = (i == laggingIndex)
				? ColorStruct { 255, 188, 160 }
				: ColorStruct { 224, 234, 246 };
			const int rowY = textY + i * MPDebugTableRowHeight;

			wchar_t nameText[64];
			wchar_t processText[24];
			wchar_t rttText[64];
			wchar_t lossText[64];
			wchar_t stallsText[64];

			swprintf_s(nameText, L"%.18ls", rows[i].Name);
			if (rows[i].Process >= 0)
				swprintf_s(processText, L"%d", rows[i].Process);
			else
				swprintf_s(processText, L"-");
			swprintf_s(rttText, L"%d/%dms", rows[i].AverageRoundTrip, rows[i].MaxRoundTrip);
			swprintf_s(lossText, L"%d%%/%d", rows[i].LossPercent, rows[i].Resends);
			swprintf_s(stallsText, L"%d/%d", rows[i].FrameSyncStalls, rows[i].CommandStalls);

			DrawMPDebugText(nameText, panelRect, colPlayer, rowY, lineColor);
			DrawMPDebugText(processText, panelRect, colProc, rowY, lineColor);
			DrawMPDebugText(rttText, panelRect, colRtt, rowY, lineColor);
			DrawMPDebugText(lossText, panelRect, colLoss, rowY, lineColor);
			DrawMPDebugText(stallsText, panelRect, colStalls, rowY, lineColor);
		}
	}
}

DEFINE_HOOK(0x777C41, UI_ApplyAppIcon, 0x9)
{
	if (Phobos::AppIconPath != nullptr && strlen(Phobos::AppIconPath))
	{
		Debug::Log("Applying AppIcon from \"%s\"\n", Phobos::AppIconPath);

		R->EAX(LoadImage(NULL, Phobos::AppIconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE));
		return 0x777C4A;
	}

	return 0;
}

DEFINE_HOOK(0x640B8D, LoadingScreen_DisableEmptySpawnPositions, 0x6)
{
	GET(const bool, esi, ESI);
	if (Phobos::UI::DisableEmptySpawnPositions || !esi)
	{
		return 0x640CE2;
	}
	return 0x640B93;
}

//DEFINE_HOOK(0x640E78, LoadingScreen_DisableColorPoints, 0x6)
//{
//	return 0x641071;
//}

// Allow size = 0 for map previews
DEFINE_HOOK(0x641B41, LoadingScreen_SkipPreview, 0x8)
{
	GET(RectangleStruct*, pRect, EAX);
	if (pRect->Width > 0 && pRect->Height > 0)
	{
		return 0;
	}
	return 0x641D4E;
}

DEFINE_HOOK(0x641EE0, PreviewClass_ReadPreview, 0x6)
{
	GET(PreviewClass*, pThis, ECX);
	GET_STACK(const char*, lpMapFile, 0x4);

	CCFileClass file(lpMapFile);
	if (file.Exists() && file.Open(FileAccessMode::Read))
	{
		CCINIClass ini;
		ini.ReadCCFile(&file, true);
		ini.CurrentSection = nullptr;
		ini.CurrentSectionName = nullptr;

		ScenarioClass::Instance->ReadStartPoints(ini);

		R->EAX(pThis->ReadPreviewPack(ini));
	}
	else
		R->EAX(false);

	return 0x64203D;
}

DEFINE_HOOK(0x4A25E0, CreditsClass_GraphicLogic_HarvesterCounter, 0x7)
{
	auto const pPlayer = HouseClass::CurrentPlayer;
	if (pPlayer->Defeated)
		return 0;

	RectangleStruct vRect = DSurface::Sidebar->GetRect();

	if (Phobos::UI::HarvesterCounter_Show && Phobos::Config::ShowHarvesterCounter)
	{
		const auto pSideExt = SideExt::ExtMap.Find(SideClass::Array.GetItem(pPlayer->SideIndex));
		wchar_t counter[0x20];
		const int nActive = HouseExt::ActiveHarvesterCount(pPlayer);
		const int nTotal = HouseExt::TotalHarvesterCount(pPlayer);
		const double nPercentage = nTotal == 0 ? 1.0 : (double)nActive / (double)nTotal;

		const ColorStruct clrToolTip = nPercentage > Phobos::UI::HarvesterCounter_ConditionYellow
			? Drawing::TooltipColor : nPercentage > Phobos::UI::HarvesterCounter_ConditionRed
			? pSideExt->Sidebar_HarvesterCounter_Yellow : pSideExt->Sidebar_HarvesterCounter_Red;

		swprintf_s(counter, L"%ls%d/%d", Phobos::UI::HarvesterLabel, nActive, nTotal);

		Point2D vPos = {
			DSurface::Sidebar->GetWidth() / 2 + 50 + pSideExt->Sidebar_HarvesterCounter_Offset.Get().X,
			2 + pSideExt->Sidebar_HarvesterCounter_Offset.Get().Y
		};

		DSurface::Sidebar->DrawText(counter, &vRect, &vPos, Drawing::RGB_To_Int(clrToolTip), 0,
			TextPrintType::UseGradPal | TextPrintType::Center | TextPrintType::Metal12);
	}

	if (Phobos::UI::PowerDelta_Show && Phobos::Config::ShowPowerDelta && pPlayer->Buildings.Count)
	{
		const auto pSideExt = SideExt::ExtMap.Find(SideClass::Array.GetItem(pPlayer->SideIndex));
		wchar_t counter[0x20];

		ColorStruct clrToolTip;

		if (pPlayer->PowerBlackoutTimer.InProgress())
		{
			clrToolTip = pSideExt->Sidebar_PowerDelta_Grey;
			swprintf_s(counter, L"%ls", Phobos::UI::PowerBlackoutLabel);
		}
		else
		{
			const int delta = pPlayer->PowerOutput - pPlayer->PowerDrain;

			const double percent = pPlayer->PowerOutput != 0
				? (double)pPlayer->PowerDrain / (double)pPlayer->PowerOutput : pPlayer->PowerDrain != 0
				? Phobos::UI::PowerDelta_ConditionRed * 2.f : Phobos::UI::PowerDelta_ConditionYellow;

			clrToolTip = percent < Phobos::UI::PowerDelta_ConditionYellow
				? pSideExt->Sidebar_PowerDelta_Green : LESS_EQUAL(percent, Phobos::UI::PowerDelta_ConditionRed)
				? pSideExt->Sidebar_PowerDelta_Yellow : pSideExt->Sidebar_PowerDelta_Red;

			swprintf_s(counter, L"%ls%+d", Phobos::UI::PowerLabel, delta);
		}

		Point2D vPos = {
			DSurface::Sidebar->GetWidth() / 2 - 70 + pSideExt->Sidebar_PowerDelta_Offset.Get().X,
			2 + pSideExt->Sidebar_PowerDelta_Offset.Get().Y
		};

		auto const TextFlags = static_cast<TextPrintType>(static_cast<int>(TextPrintType::UseGradPal | TextPrintType::Metal12)
				| static_cast<int>(pSideExt->Sidebar_PowerDelta_Align.Get()));

		DSurface::Sidebar->DrawText(counter, &vRect, &vPos, Drawing::RGB_To_Int(clrToolTip), 0, TextFlags);
	}

	if (Phobos::UI::WeedsCounter_Show && Phobos::Config::ShowWeedsCounter)
	{
		const auto pSideExt = SideExt::ExtMap.Find(SideClass::Array.GetItem(pPlayer->SideIndex));
		wchar_t counter[0x20];
		const ColorStruct clrToolTip = pSideExt->Sidebar_WeedsCounter_Color.Get(Drawing::TooltipColor);

		swprintf_s(counter, L"%d", static_cast<int>(pPlayer->OwnedWeed.GetTotalAmount()));

		Point2D vPos = {
			DSurface::Sidebar->GetWidth() / 2 + 50 + pSideExt->Sidebar_WeedsCounter_Offset.Get().X,
			2 + pSideExt->Sidebar_WeedsCounter_Offset.Get().Y
		};

		DSurface::Sidebar->DrawText(counter, &vRect, &vPos, Drawing::RGB_To_Int(clrToolTip), 0,
			TextPrintType::UseGradPal | TextPrintType::Center | TextPrintType::Metal12);
	}

	return 0;
}

DEFINE_HOOK_AGAIN(0x6CE8AA, Replace_XXICON_With_New, 0x7)   //SWTypeClass::Load
DEFINE_HOOK_AGAIN(0x6CEE31, Replace_XXICON_With_New, 0x7)   //SWTypeClass::ReadINI
DEFINE_HOOK_AGAIN(0x716D13, Replace_XXICON_With_New, 0x7)   //TechnoTypeClass::Load
DEFINE_HOOK(0x715A4D, Replace_XXICON_With_New, 0x7)         //TechnoTypeClass::ReadINI
{
	char pFilename[0x20];
	strcpy_s(pFilename, RulesExt::Global()->MissingCameo.data());
	_strlwr_s(pFilename);

	if (_stricmp(pFilename, GameStrings::XXICON_SHP)
		&& strstr(pFilename, ".shp"))
	{
		if (const auto pFile = FileSystem::LoadFile(RulesExt::Global()->MissingCameo, false))
		{
			R->EAX(pFile);
			return R->Origin() + 0xC;
		}
	}

	return 0;
}

DEFINE_HOOK(0x6A8463, StripClass_OperatorLessThan_CameoPriority, 0x5)
{
	GET_STACK(TechnoTypeClass*, pLeft, STACK_OFFSET(0x1C, -0x8));
	GET_STACK(TechnoTypeClass*, pRight, STACK_OFFSET(0x1C, -0x4));
	GET_STACK(const int, idxLeft, STACK_OFFSET(0x1C, 0x8));
	GET_STACK(const int, idxRight, STACK_OFFSET(0x1C, 0x10));
	GET_STACK(const AbstractType, rttiLeft, STACK_OFFSET(0x1C, 0x4));
	GET_STACK(const AbstractType, rttiRight, STACK_OFFSET(0x1C, 0xC));
	const auto pLeftTechnoExt = TechnoTypeExt::ExtMap.TryFind(pLeft);
	const auto pRightTechnoExt = TechnoTypeExt::ExtMap.TryFind(pRight);
	const auto pLeftSWExt = (rttiLeft == AbstractType::Special || rttiLeft == AbstractType::Super || rttiLeft == AbstractType::SuperWeaponType)
		? SWTypeExt::ExtMap.TryFind(SuperWeaponTypeClass::Array.GetItem(idxLeft)) : nullptr;
	const auto pRightSWExt = (rttiRight == AbstractType::Special || rttiRight == AbstractType::Super || rttiRight == AbstractType::SuperWeaponType)
		? SWTypeExt::ExtMap.TryFind(SuperWeaponTypeClass::Array.GetItem(idxRight)) : nullptr;

	if ((pLeftTechnoExt || pLeftSWExt) && (pRightTechnoExt || pRightSWExt))
	{
		const int leftPriority = pLeftTechnoExt ? pLeftTechnoExt->CameoPriority : pLeftSWExt->CameoPriority;
		const int rightPriority = pRightTechnoExt ? pRightTechnoExt->CameoPriority : pRightSWExt->CameoPriority;
		enum { rTrue = 0x6A8692, rFalse = 0x6A86A0 };

		if (leftPriority > rightPriority)
			return rTrue;
		else if (rightPriority > leftPriority)
			return rFalse;
	}

	// Restore overridden instructions
	GET(const AbstractType, rtti1, ESI);
	return rtti1 == AbstractType::Special ? 0x6A8477 : 0x6A8468;
}

DEFINE_HOOK(0x6A84DB, StripClass_OperatorLessThan_SortCameoByNameSW, 0x5)
{
	enum { rTrue = 0x6A8692, rFalse = 0x6A86A0 };

	GET(SuperWeaponTypeClass*, pLeftSW, EAX);
	GET(SuperWeaponTypeClass*, pRightSW, ECX);

	if (RulesExt::Global()->SortCameoByName)
	{
		const int result = strcmp(pLeftSW->Name, pRightSW->Name);

		if (result < 0)
			return rTrue;
		else if (result > 0)
			return rFalse;
	}

	return wcscmp(pLeftSW->UIName, pRightSW->UIName) <= 0 ? rTrue : rFalse;
}

DEFINE_HOOK(0x6A86ED, StripClass_OperatorLessThan_SortCameoByNameTechno, 0x5)
{
	enum { rTrue = 0x6A8692, rFalse = 0x6A86A0 };

	GET(TechnoTypeClass*, pLeft, EDI);
	GET(TechnoTypeClass*, pRight, EBP);

	if (RulesExt::Global()->SortCameoByName)
	{
		const int result = strcmp(pLeft->Name, pRight->Name);

		if (result < 0)
			return rTrue;
		else if (result > 0)
			return rFalse;
	}

	return wcscmp(pLeft->UIName, pRight->UIName) <= 0 ? rTrue : rFalse;
}

DEFINE_HOOK(0x6D4684, TacticalClass_Draw_FlyingStrings, 0x6)
{
	FlyingStrings::UpdateAll();
	return 0;
}

DEFINE_HOOK(0x456776, BuildingClass_DrawRadialIndicator_Visibility, 0x6)
{
	enum { ContinueDraw = 0x456789, DoNotDraw = 0x456962 };
	GET(BuildingClass* const, pThis, ESI);

	if (HouseClass::IsCurrentPlayerObserver() || pThis->Owner->IsControlledByCurrentPlayer())
		return ContinueDraw;

	AffectedHouse const canSee = RulesExt::Global()->RadialIndicatorVisibility.Get();
	if (pThis->Owner->IsAlliedWith(HouseClass::CurrentPlayer) ? canSee & AffectedHouse::Allies : canSee & AffectedHouse::Enemies)
		return ContinueDraw;

	return DoNotDraw;
}

DEFINE_HOOK(0x6D4B25, TacticalClass_Render_Banner, 0x5)
{
	for (const auto& pBanner : BannerClass::Array)
		pBanner->Render();

	return 0;
}

#pragma region ShowBriefing

namespace BriefingTemp
{
	bool ShowBriefing = false;
}

static __forceinline void ShowBriefing()
{
	if (BriefingTemp::ShowBriefing)
	{
		// Show briefing dialog.
		Game::SpecialDialog = 9;
		Game::ShowSpecialDialog();
		BriefingTemp::ShowBriefing = false;

		// Play scenario theme.
		const int theme = ScenarioClass::Instance->ThemeIndex;

		if (theme == -1)
			ThemeClass::Instance.Stop(true);
		else
			ThemeClass::Instance.Queue(theme);
	}
}

// Check if briefing dialog should be played before starting scenario.
DEFINE_HOOK(0x683E41, ScenarioClass_Start_ShowBriefing, 0x6)
{
	enum { SkipGameCode = 0x683E6B };

	GET_STACK(const bool, showBriefing, STACK_OFFSET(0xFC, -0xE9));

	// Don't show briefing dialog for non-campaign games etc.
	if (!Phobos::Config::ShowBriefing || !ScenarioExt::Global()->ShowBriefing || !showBriefing || !SessionClass::IsCampaign())
		return 0;

	BriefingTemp::ShowBriefing = true;

	int theme = ScenarioExt::Global()->BriefingTheme;

	if (theme == -1)
	{
		const SideClass* pSide = SideClass::Array.GetItemOrDefault(ScenarioClass::Instance->PlayerSideIndex);

		if (const auto pSideExt = SideExt::ExtMap.TryFind(pSide))
			theme = pSideExt->BriefingTheme;
	}

	if (theme != -1)
		ThemeClass::Instance.Queue(theme);

	// Skip over playing scenario theme.
	return SkipGameCode;
}

// Show the briefing dialog before entering game loop.
DEFINE_HOOK(0x48CE85, MainGame_ShowBriefing, 0x5)
{
	enum { SkipGameCode = 0x48CE8A };

	// Restore overridden instructions.
	SessionClass::Instance.Resume();

	ShowBriefing();

	return SkipGameCode;
}

// Show the briefing dialog on starting a new scenario after clearing another.
DEFINE_HOOK(0x55D14F, AuxLoop_ShowBriefing, 0x5)
{
	enum { SkipGameCode = 0x55D159 };

	// Restore overridden instructions.
	SessionClass::Instance.Resume();

	ShowBriefing();

	return SkipGameCode;
}

// Skip redrawing the screen if we're gonna show the briefing screen immediately after loading screen finishes on initially launched mission.
DEFINE_HOOK(0x683F66, PauseGame_ShowBriefing, 0x5)
{
	enum { SkipGameCode = 0x683FAA };

	if (BriefingTemp::ShowBriefing)
		return SkipGameCode;

	return 0;
}

// Skip redrawing the screen if we're gonna show the briefing screen immediately after loading screen finishes on succeeding missions.
DEFINE_HOOK(0x685D95, DoWin_ShowBriefing, 0x5)
{
	enum { SkipGameCode = 0x685D9F };

	if (BriefingTemp::ShowBriefing)
		return SkipGameCode;

	return 0;
}

// Set briefing dialog resume button text.
DEFINE_HOOK(0x65F764, BriefingDialog_ShowBriefing, 0x5)
{
	if (BriefingTemp::ShowBriefing)
	{
		GET(const HWND, hDlg, ESI);

		auto const hResumeBtn = GetDlgItem(hDlg, 1059);
		SendMessageA(hResumeBtn, 1202, 0, reinterpret_cast<LPARAM>(Phobos::UI::ShowBriefingResumeButtonLabel));
	}

	return 0;
}

// Set briefing dialog resume button status bar label.
DEFINE_HOOK(0x604985, GetDialogUIStatusLabels_ShowBriefing, 0x5)
{
	if (BriefingTemp::ShowBriefing)
	{
		enum { SkipGameCode = 0x60498A };

		R->EAX(Phobos::UI::ShowBriefingResumeButtonStatusLabel);

		return SkipGameCode;
	}

	return 0;
}

#pragma endregion

static bool __fastcall Fake_HouseIsAlliedWith(HouseClass* pThis, void*, HouseClass* CurrentPlayer)
{
	return (Phobos::Config::ShowPlanningPath && SessionClass::IsSingleplayer())
		|| pThis->IsControlledByCurrentPlayer()
		|| pThis->IsAlliedWith(CurrentPlayer);
}

DEFINE_FUNCTION_JUMP(CALL, 0x63B136, Fake_HouseIsAlliedWith);
DEFINE_FUNCTION_JUMP(CALL, 0x63B100, Fake_HouseIsAlliedWith);
DEFINE_FUNCTION_JUMP(CALL, 0x63B17F, Fake_HouseIsAlliedWith);
DEFINE_FUNCTION_JUMP(CALL, 0x63B1BA, Fake_HouseIsAlliedWith);
DEFINE_FUNCTION_JUMP(CALL, 0x63B2CE, Fake_HouseIsAlliedWith);

DEFINE_HOOK(0x69A317, SessionClass_PlayerColorIndexToColorSchemeIndex, 0x0)
{
	GET_STACK(int, index, 0x4);

	const bool isRandom = index == PlayerColorSlot::Random;

	if (Phobos::Config::SkirmishUnlimitedColors)
	{
		// Allow player color indices to map directly to color scheme indices.
		if (isRandom)
			index = ColorScheme::FindIndex("LightGrey", 53);
		else
			index = index * 2 + 1;
	}
	else
	{
		// Vanilla behaviour.
		if (isRandom)
			index = ColorScheme::PlayerColorToColorSchemeLUT[PlayerColorSlot::White];
		else if (index < PlayerColorSlot::Count)
			index = ColorScheme::PlayerColorToColorSchemeLUT[index];
	}

	R->EAX(index);

	return 0x69A325;
}

DEFINE_HOOK(0x552F79, LoadProgressManager_Draw_MissingLoadingScreenDefaults, 0x6)
{
	GET(LoadProgressManager*, pThis, EBP);
	GET(ConvertClass*, pDrawer, EBX);
	GET_STACK(const bool, isLowRes, STACK_OFFSET(0x1268, -0x1235));

	auto const pScenarioExt = ScenarioExt::Global();

	if (!pThis->LoadScreenSHP)
		pThis->LoadScreenSHP = FileSystem::LoadSHPFile(isLowRes ? pScenarioExt->DefaultLS640BkgdName : pScenarioExt->DefaultLS800BkgdName);

	if (!pDrawer)
	{
		// Uncertain how necessary this is but is what game does...
		if (LoadProgressManager::LoadScreenPal)
		{
			GameDelete(LoadProgressManager::LoadScreenPal);
			LoadProgressManager::LoadScreenPal = nullptr;
		}

		if (LoadProgressManager::LoadScreenBytePal)
		{
			GameDelete(LoadProgressManager::LoadScreenBytePal);
			LoadProgressManager::LoadScreenBytePal = nullptr;
		}

		ConvertClass::CreateFromFile(pScenarioExt->DefaultLS800BkgdPal, LoadProgressManager::LoadScreenBytePal, LoadProgressManager::LoadScreenPal);

		R->EBX(LoadProgressManager::LoadScreenPal);
	}

	return 0;
}

// Replaces vanilla MP debug stats rendering with a compact custom panel.
// Hooking at 0x55F1F2 catches both early branches so custom UI appears
// immediately when the debug flag is active.
DEFINE_HOOK(0x55F1F2, MPDebugPrint_CheckDrawFlag, 0x6)
{
	if (!Game::DrawMPDebugStats)
		return 0x55F67C;

	// 0x55F1F2 is inside the vanilla MP debug print routine after prologue. We replace its
	// rendering entirely and jump to the function epilogue.
	DrawCompactMPDebugStats();
	return 0x55F67C;
}
