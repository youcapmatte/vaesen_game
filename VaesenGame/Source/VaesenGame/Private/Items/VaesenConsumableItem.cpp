#include "Items/VaesenConsumableItem.h"

#define LOCTEXT_NAMESPACE "VaesenItems"

UVaesenConsumableItem::UVaesenConsumableItem()
{
	UseActionText = LOCTEXT("ConsumableUse", "Consume");
	MaxStackSize = 10;
}

#undef LOCTEXT_NAMESPACE
