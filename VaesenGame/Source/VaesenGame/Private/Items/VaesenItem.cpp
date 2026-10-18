#include "Items/VaesenItem.h"

#include "Net/UnrealNetwork.h"

void UVaesenItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenItem, Quantity);
}

void UVaesenItem::SetQuantity(int32 NewQuantity)
{
	const int32 Clamped = FMath::Clamp(NewQuantity, 1, GetMaxStack());
	if (Clamped != Quantity)
	{
		Quantity = Clamped;
		OnItemModified.Broadcast();
	}
}

void UVaesenItem::OnRep_Quantity()
{
	OnItemModified.Broadcast();
}

#if WITH_EDITOR
void UVaesenItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Not stackable: one per stack (MaxStackSize is greyed out in the details panel meanwhile).
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UVaesenItem, bStackable) && !bStackable)
	{
		MaxStackSize = 1;
	}
}
#endif
