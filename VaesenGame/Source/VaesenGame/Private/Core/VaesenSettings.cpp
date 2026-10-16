#include "Core/VaesenSettings.h"

UVaesenSettings::UVaesenSettings()
{
	ApiBaseUrl = TEXT("http://localhost:5230");
	LoginMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Vaesen/Maps/L_Login.L_Login")));
	CharacterSelectMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Vaesen/Maps/L_CharacterSelect.L_CharacterSelect")));
}
