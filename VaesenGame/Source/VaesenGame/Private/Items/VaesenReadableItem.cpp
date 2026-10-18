#include "Items/VaesenReadableItem.h"

#define LOCTEXT_NAMESPACE "VaesenItems"

UVaesenReadableItem::UVaesenReadableItem()
{
	UseActionText = LOCTEXT("ReadableUse", "Read");
	bStackable = false;
	MaxStackSize = 1;
}

#undef LOCTEXT_NAMESPACE
