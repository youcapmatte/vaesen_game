#include "Online/VaesenServerApiSubsystem.h"

#include "Core/VaesenSettings.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogVaesenServerApi);

bool UVaesenServerApiSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVaesenServerApiSubsystem::ReportCharacterLeft(const FString& CharacterId)
{
	if (!CharacterId.IsEmpty())
	{
		Post(FString::Printf(TEXT("/api/sessions/server/characters/%s/left"), *FGenericPlatformHttp::UrlEncode(CharacterId)), TEXT("{}"));
	}
}

void UVaesenServerApiSubsystem::SendHeartbeat(const TArray<FString>& CharacterIds)
{
	if (CharacterIds.IsEmpty())
	{
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Ids;
	for (const FString& Id : CharacterIds)
	{
		Ids.Add(MakeShared<FJsonValueString>(Id));
	}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("characterIds"), Ids);

	FString Json;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));
	Post(TEXT("/api/sessions/server/heartbeat"), Json);
}

void UVaesenServerApiSubsystem::Post(const FString& Path, const FString& Body)
{
	// No machine credentials yet: the API leaves these endpoints open in Development.
	const auto Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(UVaesenSettings::Get()->ApiBaseUrl + Path);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);
	Request->OnProcessRequestComplete().BindLambda([Path](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		if (!bConnected || !Response.IsValid())
		{
			UE_LOG(LogVaesenServerApi, Warning, TEXT("POST %s: couldn't reach the API."), *Path);
		}
		else if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			UE_LOG(LogVaesenServerApi, Warning, TEXT("POST %s: %d %s"), *Path, Response->GetResponseCode(), *Response->GetContentAsString());
		}
	});
	Request->ProcessRequest();
}
