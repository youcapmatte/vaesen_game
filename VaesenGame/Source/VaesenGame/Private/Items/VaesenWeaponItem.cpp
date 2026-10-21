#include "Items/VaesenWeaponItem.h"

#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/VaesenMeleeWeaponActor.h"
#include "Weapons/VaesenWeaponActor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "VaesenWeapons"

UVaesenWeaponItem::UVaesenWeaponItem()
{
	Slot = EVaesenEquipmentSlot::MainHand;
	AttachSocket = NAME_None;
}

void UVaesenWeaponItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenWeaponItem, CurrentDurability);
	DOREPLIFETIME(UVaesenWeaponItem, WeaponActor);
}

UVaesenItem* UVaesenWeaponItem::DuplicateItem(UObject* NewOuter, int32 NewQuantity) const
{
	UVaesenWeaponItem* Copy = CastChecked<UVaesenWeaponItem>(Super::DuplicateItem(NewOuter, NewQuantity));
	Copy->WeaponActor = nullptr;
	Copy->OnDurabilityChanged.Clear();
	return Copy;
}

const FVaesenCategoryDefaults& UVaesenWeaponItem::GetCategoryDefaults() const
{
	return UVaesenWeaponCategoryDefaults::Get()->GetCategory(Category);
}

TSubclassOf<UAnimInstance> UVaesenWeaponItem::GetAnimLayer() const
{
	return AnimLayerOverride ? AnimLayerOverride : GetCategoryDefaults().AnimLayer;
}

UAnimMontage* UVaesenWeaponItem::GetMontage(TObjectPtr<UAnimMontage> FVaesenWeaponMontages::* Which) const
{
	if (UAnimMontage* Own = MontageOverrides.*Which)
	{
		return Own;
	}
	return GetCategoryDefaults().Montages.*Which;
}

const UVaesenWeaponSoundSet* UVaesenWeaponItem::GetSoundSetFor(EVaesenWeaponSound Sound) const
{
	if (SoundSet && SoundSet->GetSound(Sound))
	{
		return SoundSet;
	}
	return GetCategoryDefaults().Sounds;
}

USoundBase* UVaesenWeaponItem::GetSound(EVaesenWeaponSound Sound) const
{
	const UVaesenWeaponSoundSet* Set = GetSoundSetFor(Sound);
	return Set ? Set->GetSound(Sound) : nullptr;
}

UAnimMontage* UVaesenWeaponItem::GetDrawMontage(bool bDraw) const
{
	if (UAnimMontage* Own = bDraw ? DrawMontage.Get() : SheathMontage.Get())
	{
		return Own;
	}
	const FVaesenSheathLocation* Location = UVaesenWeaponCategoryDefaults::Get()->FindSheathLocation(SheathSocket);
	return Location ? (bDraw ? Location->DrawMontage.Get() : Location->SheathMontage.Get()) : nullptr;
}

UStaticMesh* UVaesenWeaponItem::GetHeldStaticMesh() const
{
	return WeaponMesh ? WeaponMesh.Get() : PickupMesh.Get();
}

TArray<FName> UVaesenWeaponItem::GetSheathSocketOptions() const
{
	return UVaesenWeaponCategoryDefaults::Get()->GetSheathSocketNames();
}

void UVaesenWeaponItem::ApplyWear(float Amount)
{
	const AActor* Owner = GetOwningActor();
	if (Amount <= 0.0f || IsBroken() || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	const bool bWasNearBreaking = IsNearBreaking();
	CurrentDurability = VaesenDamageMath::ApplyWear(GetDurability(), Amount);
	OnRep_Durability();

	if (AVaesenWeaponActor* Actor = WeaponActor.Get())
	{
		if (IsBroken())
		{
			UE_LOG(LogVaesenCombat, Log, TEXT("%s broke."), *GetName());
			Actor->MulticastPlaySound(EVaesenWeaponSound::Break);
		}
		else if (!bWasNearBreaking && IsNearBreaking())
		{
			Actor->MulticastPlaySound(EVaesenWeaponSound::NearBreak);
		}
	}
}

void UVaesenWeaponItem::Repair()
{
	const AActor* Owner = GetOwningActor();
	if (Owner && Owner->HasAuthority())
	{
		CurrentDurability = -1.0f;
		OnRep_Durability();
	}
}

void UVaesenWeaponItem::OnRep_Durability()
{
	OnDurabilityChanged.Broadcast();
}

TSubclassOf<AVaesenWeaponActor> UVaesenWeaponItem::GetDefaultActorClass() const
{
	return AVaesenMeleeWeaponActor::StaticClass();
}

void UVaesenWeaponItem::OnEquipped_Implementation(AVaesenBaseCharacter* Character)
{
	// Not the plain attached mesh (Super): the server spawns the weapon actor, which replicates to everyone.
	if (!Character->HasAuthority() || WeaponActor)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	const TSubclassOf<AVaesenWeaponActor> ActorClass = WeaponActorClass ? WeaponActorClass : GetDefaultActorClass();
	if (AVaesenWeaponActor* Actor = Character->GetWorld()->SpawnActor<AVaesenWeaponActor>(ActorClass, Character->GetActorTransform(), Params))
	{
		Actor->SetWeaponItem(this);
		Actor->FinishSpawning(Character->GetActorTransform());
		WeaponActor = Actor;
	}
}

void UVaesenWeaponItem::OnUnequipped_Implementation(AVaesenBaseCharacter* Character)
{
	if (AVaesenWeaponActor* Actor = WeaponActor.Get())
	{
		if (Actor->HasAuthority())
		{
			Actor->Destroy();
		}
	}
	if (Character && Character->HasAuthority())
	{
		WeaponActor = nullptr;
	}
}

#if WITH_EDITOR
void UVaesenWeaponItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Choosing a category sets where it's worn and carried; both can still be changed afterwards.
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UVaesenWeaponItem, Category))
	{
		switch (Category)
		{
		case EVaesenWeaponCategory::Pistol:
		case EVaesenWeaponCategory::Revolver:
			Slot = EVaesenEquipmentSlot::Sidearm;
			SheathSocket = TEXT("Sheath_HipRight");
			break;
		case EVaesenWeaponCategory::Dagger:
			Slot = EVaesenEquipmentSlot::Sidearm;
			SheathSocket = TEXT("Sheath_Belt");
			break;
		case EVaesenWeaponCategory::Shield:
			Slot = EVaesenEquipmentSlot::OffHand;
			SheathSocket = TEXT("Sheath_BackUpper");
			break;
		case EVaesenWeaponCategory::Rifle:
		case EVaesenWeaponCategory::Crossbow:
			Slot = EVaesenEquipmentSlot::MainHand;
			bTwoHanded = true;
			SheathSocket = TEXT("Sheath_BackSling");
			break;
		case EVaesenWeaponCategory::Bow:
			Slot = EVaesenEquipmentSlot::MainHand;
			bTwoHanded = true;
			SheathSocket = TEXT("Sheath_BackUpper");
			break;
		case EVaesenWeaponCategory::OneHandedSword:
			Slot = EVaesenEquipmentSlot::MainHand;
			SheathSocket = TEXT("Sheath_HipLeft");
			break;
		default:
			Slot = EVaesenEquipmentSlot::MainHand;
			SheathSocket = TEXT("Sheath_BackLower");
			break;
		}
	}
}

EDataValidationResult UVaesenWeaponItem::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const UStaticMesh* Mesh = GetHeldStaticMesh();
	const UStaticMeshSocket* Grip = Mesh ? Mesh->FindSocket(TEXT("Grip")) : nullptr;
	if (Mesh && !Grip)
	{
		Context.AddWarning(FText::Format(LOCTEXT("NoGrip", "{0}: the mesh {1} has no Grip socket; it's held by its origin."), FText::FromString(GetName()), FText::FromString(Mesh->GetName())));
	}

	// The draw animation is shared per sheath location; IK only covers small differences in where the handle is.
	const UVaesenWeaponCategoryDefaults* Defaults = UVaesenWeaponCategoryDefaults::Get();
	const FVaesenSheathLocation* Location = Defaults->FindSheathLocation(SheathSocket);
	if (!Location)
	{
		Context.AddWarning(FText::Format(LOCTEXT("UnknownSheath", "{0}: sheath socket {1} isn't in DA_WeaponCategoryDefaults."), FText::FromString(GetName()), FText::FromName(SheathSocket)));
	}
	else if (Grip && !DrawMontage)
	{
		// The actor holds the mesh so Grip is at its origin, and the origin hangs at SheathOffset in the socket.
		const FVector GripInSocket = SheathOffset.GetLocation();
		const float Distance = FVector::Dist(GripInSocket, Location->ReferenceGrabPoint);
		if (Distance > Defaults->GripWarningDistance)
		{
			Context.AddWarning(FText::Format(LOCTEXT("GripFar", "{0}: sheathed, its Grip is {1} cm from {2}'s grab point (more than {3} cm): the arm will stretch. Move it, or give it its own draw montage."),
				FText::FromString(GetName()), FText::AsNumber(FMath::RoundToInt(Distance)), FText::FromName(SheathSocket), FText::AsNumber(FMath::RoundToInt(Defaults->GripWarningDistance))));
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE

void UVaesenWeaponItem::OnRep_WeaponActor()
{
	const AVaesenBaseCharacter* Character = Cast<AVaesenBaseCharacter>(GetOwningActor());
	if (UVaesenEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr)
	{
		Equipment->RefreshWeaponAttachments();
	}
}
