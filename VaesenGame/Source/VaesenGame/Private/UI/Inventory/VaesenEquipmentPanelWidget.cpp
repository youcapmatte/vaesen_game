#include "UI/Inventory/VaesenEquipmentPanelWidget.h"

#include "Characters/VaesenBaseCharacter.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "UI/Inventory/VaesenCharacterPreview.h"
#include "UI/Inventory/VaesenEquipmentSlotWidget.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"

void UVaesenEquipmentPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!PreviewClass)
	{
		PreviewClass = AVaesenCharacterPreview::StaticClass();
	}
	SpawnPreview();
}

void UVaesenEquipmentPanelWidget::NativeDestruct()
{
	if (Equipment)
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UVaesenEquipmentPanelWidget::Refresh);
	}
	DestroyPreview();
	Super::NativeDestruct();
}

void UVaesenEquipmentPanelWidget::SetEquipment(UVaesenEquipmentComponent* InEquipment)
{
	if (Equipment == InEquipment && SlotWidgets.Num() > 0)
	{
		Refresh();
		return;
	}

	if (Equipment)
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UVaesenEquipmentPanelWidget::Refresh);
	}
	Equipment = InEquipment;
	if (Equipment)
	{
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UVaesenEquipmentPanelWidget::Refresh);
	}

	SlotWidgets.Reset();
	for (UPanelWidget* Column : { LeftSlots.Get(), RightSlots.Get() })
	{
		if (Column)
		{
			Column->ClearChildren();
		}
	}

	// Armor down the left (head to feet), everything else down the right.
	const TArray<EVaesenEquipmentSlot> Slots = UVaesenEquipmentComponent::GetPlayerSlots();
	if (Equipment && SlotWidgetClass)
	{
		for (const EVaesenEquipmentSlot EquipmentSlot : Slots)
		{
			const bool bArmor = IsArmorSlot(EquipmentSlot) && EquipmentSlot != EVaesenEquipmentSlot::Back;
			UPanelWidget* Column = bArmor ? LeftSlots.Get() : RightSlots.Get();
			if (!Column)
			{
				continue;
			}
			UVaesenEquipmentSlotWidget* SlotWidget = CreateWidget<UVaesenEquipmentSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
			SlotWidget->SetSlot(Equipment, EquipmentSlot);
			UPanelSlot* PanelSlot = Column->AddChild(SlotWidget);
			if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
			{
				BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, SlotSpacing));
				BoxSlot->SetHorizontalAlignment(HAlign_Center);
			}
			SlotWidgets.Add(SlotWidget);
		}
	}
	Refresh();
}

void UVaesenEquipmentPanelWidget::Refresh()
{
	for (UVaesenEquipmentSlotWidget* SlotWidget : SlotWidgets)
	{
		SlotWidget->Refresh();
	}
	if (Preview && Equipment)
	{
		Preview->CopyAppearance(Cast<AVaesenBaseCharacter>(Equipment->GetOwner()));
	}
}

void UVaesenEquipmentPanelWidget::SpawnPreview()
{
	UWorld* World = GetWorld();
	if (Preview || !World || !PreviewClass || !PreviewImage)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	Preview = World->SpawnActor<AVaesenCharacterPreview>(PreviewClass, FTransform(AVaesenCharacterPreview::GetStudioLocation()), SpawnParameters);
	if (!Preview)
	{
		return;
	}

	PreviewImage->SetBrushResourceObject(Preview->GetRenderTarget());
	if (Equipment)
	{
		Preview->CopyAppearance(Cast<AVaesenBaseCharacter>(Equipment->GetOwner()));
	}
}

void UVaesenEquipmentPanelWidget::DestroyPreview()
{
	if (PreviewImage)
	{
		PreviewImage->SetBrushResourceObject(nullptr);
	}
	if (Preview)
	{
		Preview->Destroy();
		Preview = nullptr;
	}
}

FReply UVaesenEquipmentPanelWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Left-drag on the character turns it.
	const bool bOnCharacter = PreviewImage && PreviewImage->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	if (Preview && bOnCharacter && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bTurning = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVaesenEquipmentPanelWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bTurning && Preview)
	{
		Preview->AddYaw(InMouseEvent.GetCursorDelta().X * TurnSpeed);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UVaesenEquipmentPanelWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bTurning)
	{
		bTurning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

bool UVaesenEquipmentPanelWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// An item from an inventory: on, in its own slot.
	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	if (Operation && !Operation->bFromEquipment && Operation->SourceInventory && Equipment
		&& Equipment->CanEquip(Operation->SourceInventory->GetItemAt(Operation->SourceIndex)))
	{
		Equipment->RequestEquipFromInventory(Operation->SourceInventory, Operation->SourceIndex);
	}
	return true;
}
