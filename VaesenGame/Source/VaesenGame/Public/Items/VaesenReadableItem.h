#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "VaesenReadableItem.generated.h"

class UTexture2D;

/** Letters, journals, books, newspaper cuttings, clues: opens a reading view. Clues are usually quest items. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenReadableItem : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenReadableItem();

	/** The text, one entry per page. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Readable", meta = (MultiLine = "true"))
	TArray<FText> Pages;

	/** Optional picture (a drawing, a photograph, a map) shown with the text. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Readable")
	TObjectPtr<UTexture2D> Illustration;

	/** Part of an investigation: reading it adds it to the case notes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Readable")
	bool bIsClue = false;
};
