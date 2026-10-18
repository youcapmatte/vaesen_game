#include "Items/VaesenToolItem.h"

#define LOCTEXT_NAMESPACE "VaesenItems"

UVaesenToolItem::UVaesenToolItem()
{
	Slot = EVaesenEquipmentSlot::OffHand;
	AttachSocket = TEXT("hand_l");
	UseActionText = LOCTEXT("ToolUse", "Use");
}

#undef LOCTEXT_NAMESPACE
