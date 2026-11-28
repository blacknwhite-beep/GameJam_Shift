// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FarmGraveActor.generated.h"

class USceneComponent;
class UBoxComponent;
class ADeadCivilianBody;

/**
 * A 6ft+ grave-sized freshly-dug open pit located behind the Red Barn in the Farm dimension.
 * When the player is near the grave, looking at it, and carrying a dead civilian body on their
 * shoulder, Left-Clicking dumps the body into the grave.
 */
UCLASS()
class GAMEJAM_SHIFT_2026_API AFarmGraveActor : public AActor
{
	GENERATED_BODY()

public:
	AFarmGraveActor();

	/** Dumps the carried civilian body into this grave pit. */
	void DumpBodyIntoGrave(ADeadCivilianBody* Body);

	/** Number of bodies currently dumped in this grave. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grave")
	int32 DumpedBodyCount = 0;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Grave")
	TObjectPtr<USceneComponent> RootScene;

	/** Generous visibility-blocking box so looking anywhere at the grave triggers dump interaction. */
	UPROPERTY(VisibleAnywhere, Category = "Grave")
	TObjectPtr<UBoxComponent> GraveInteractBounds;

	void BuildGraveVisuals();
};
