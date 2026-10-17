#include "Core/VaesenSessionSubsystem.h"

bool UVaesenSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer();
}

void UVaesenSessionSubsystem::Logout()
{
	Account = FVaesenAccount();
	SelectedGuild = FVaesenGuildRef();
	SelectedCharacter = FVaesenCharacterRef();
	AccessToken.Reset();
	OnSessionChanged.Broadcast();
}

void UVaesenSessionSubsystem::SetAccount(const FVaesenAccount& InAccount)
{
	Account = InAccount;
	SelectedGuild = FVaesenGuildRef();
	SelectedCharacter = FVaesenCharacterRef();
	AccessToken.Reset();
	OnSessionChanged.Broadcast();
}

void UVaesenSessionSubsystem::SetSelectedGuild(const FVaesenGuildRef& Guild)
{
	SelectedGuild = Guild;
	OnSessionChanged.Broadcast();
}

void UVaesenSessionSubsystem::SetSelectedCharacter(const FVaesenCharacterRef& Character, const FString& InAccessToken)
{
	SelectedCharacter = Character;
	AccessToken = InAccessToken;
	OnSessionChanged.Broadcast();
}
