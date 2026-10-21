#include "CharacterCreator/VaesenCharacterCreatorPlayerController.h"

#include "CharacterCreator/VaesenCharacterCreatorScene.h"
#include "CharacterCreator/VaesenCharacterCreatorWidget.h"
#include "EngineUtils.h"

TSubclassOf<UUserWidget> AVaesenCharacterCreatorPlayerController::GetDefaultScreenWidgetClass() const
{
	return UVaesenCharacterCreatorWidget::StaticClass();
}

void AVaesenCharacterCreatorPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	// The panel takes the mouse; the arrow keys still reach the controller.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	TActorIterator<AVaesenCharacterCreatorScene> It(GetWorld());
	Scene = It ? *It : nullptr;
	if (!Scene)
	{
		UE_LOG(LogTemp, Warning, TEXT("Character creator: no AVaesenCharacterCreatorScene in %s"), *GetNameSafe(GetWorld()));
		return;
	}
	if (!Scene->GetCreator())
	{
		Scene->Open(this, StartSex);
	}
	if (UVaesenCharacterCreatorWidget* const Panel = Cast<UVaesenCharacterCreatorWidget>(ScreenWidget))
	{
		Panel->SetScene(Scene);
	}
}

void AVaesenCharacterCreatorPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!Scene)
	{
		return;
	}
	const float Direction = (IsInputKeyDown(EKeys::Right) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::Left) ? 1.f : 0.f);
	if (Direction != 0.f)
	{
		Scene->AddTurntableYaw(Direction * TurnSpeed * DeltaTime);
	}
}
