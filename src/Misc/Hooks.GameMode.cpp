#include <Ext/Rules/Body.h>
#include <Unsorted.h>

DEFINE_HOOK(0x5D7035, MPGameModeClass_SpawnBaseUnits_NoStartingBaseUnits, 0x5)
{
	R->EAX(static_cast<int>(!RulesExt::Global()->NoStartingBaseUnits && Unsorted::Bases));
	return 0;
}
