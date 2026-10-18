#include "UI/Inventory/VaesenItemTooltipWidget.h"

#include "Components/TextBlock.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenArmorItem.h"
#include "Items/VaesenConsumableItem.h"
#include "Items/VaesenContainerItem.h"
#include "Items/VaesenIngredientItem.h"
#include "Items/VaesenPouchItem.h"
#include "Items/VaesenReadableItem.h"
#include "Items/VaesenTalismanItem.h"
#include "Items/VaesenToolItem.h"
#include "Items/VaesenWeaponItem.h"

#define LOCTEXT_NAMESPACE "VaesenItemTooltip"

void UVaesenItemTooltipWidget::SetItem(UVaesenItem* InItem)
{
	Item = InItem;
	UpdateTooltip();
}

FText UVaesenItemTooltipWidget::GetCategoryText(const UVaesenItem* Item)
{
	if (!Item)
	{
		return FText::GetEmpty();
	}
	if (Item->bQuestItem)
	{
		return LOCTEXT("Quest", "Quest item");
	}
	if (const UVaesenConsumableItem* Consumable = Cast<UVaesenConsumableItem>(Item))
	{
		return UEnum::GetDisplayValueAsText(Consumable->ConsumableType);
	}
	if (Item->IsA<UVaesenPouchItem>())      { return LOCTEXT("Pouch", "Pouch"); }
	if (Item->IsA<UVaesenWeaponItem>())     { return LOCTEXT("Weapon", "Weapon"); }
	if (Item->IsA<UVaesenArmorItem>())      { return LOCTEXT("Armor", "Clothing"); }
	if (Item->IsA<UVaesenToolItem>())       { return LOCTEXT("Tool", "Tool"); }
	if (Item->IsA<UVaesenTalismanItem>())   { return LOCTEXT("Talisman", "Talisman"); }
	if (Item->IsA<UVaesenContainerItem>())  { return LOCTEXT("Container", "Container"); }
	if (Item->IsA<UVaesenAmmoItem>())       { return LOCTEXT("Ammo", "Ammunition"); }
	if (Item->IsA<UVaesenReadableItem>())   { return LOCTEXT("Readable", "Document"); }
	if (Item->IsA<UVaesenIngredientItem>()) { return LOCTEXT("Ingredient", "Ingredient"); }
	return LOCTEXT("Misc", "Miscellaneous");
}

void UVaesenItemTooltipWidget::UpdateTooltip_Implementation()
{
	if (!Item)
	{
		return;
	}

	const auto SetLine = [](UTextBlock* Line, const FText& Text)
	{
		if (Line)
		{
			Line->SetText(Text);
			Line->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	};

	SetLine(NameText, Item->ItemName);
	SetLine(CategoryText, GetCategoryText(Item));
	SetLine(DescriptionText, Item->Description);

	FNumberFormattingOptions OneDecimal;
	OneDecimal.MinimumFractionalDigits = 0;
	OneDecimal.MaximumFractionalDigits = 1;

	TArray<FText> Lines;

	// What's in a pouch, one line per stack.
	if (const UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(Item))
	{
		Lines.Add(FText::Format(LOCTEXT("PouchSlots", "Holds {0} / {1}"), FText::AsNumber(Pouch->GetUsedPouchSlots()), FText::AsNumber(Pouch->PouchSlots)));
		for (const UVaesenItem* Inner : Pouch->GetContents())
		{
			if (Inner)
			{
				Lines.Add(FText::Format(LOCTEXT("PouchLine", "  {0} x {1}"), FText::AsNumber(Inner->GetQuantity()), Inner->ItemName));
			}
		}
	}

	if (Item->IsA<UVaesenPouchItem>())
	{
		Lines.Add(FText::Format(LOCTEXT("PouchWeight", "Weight {0} kg"), FText::AsNumber(Item->GetStackWeight(), &OneDecimal)));
	}
	else if (Item->Weight > 0.0f)
	{
		Lines.Add(Item->GetQuantity() > 1
			? FText::Format(LOCTEXT("WeightStack", "Weight {0} kg ({1} kg)"), FText::AsNumber(Item->Weight, &OneDecimal), FText::AsNumber(Item->GetStackWeight(), &OneDecimal))
			: FText::Format(LOCTEXT("Weight", "Weight {0} kg"), FText::AsNumber(Item->Weight, &OneDecimal)));
	}
	if (Item->Value > 0)
	{
		Lines.Add(FText::Format(LOCTEXT("Value", "Value {0}"), FText::AsNumber(Item->Value)));
	}
	if (Item->GetMaxStack() > 1)
	{
		Lines.Add(FText::Format(LOCTEXT("Stack", "Stack {0} / {1}"), FText::AsNumber(Item->GetQuantity()), FText::AsNumber(Item->GetMaxStack())));
	}
	if (!Item->UseActionText.IsEmpty())
	{
		Lines.Add(FText::Format(LOCTEXT("UseHint", "Right-click: {0}"), Item->UseActionText));
	}
	SetLine(StatsText, FText::Join(FText::FromString(TEXT("\n")), Lines));
}

#undef LOCTEXT_NAMESPACE
