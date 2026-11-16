// Copyright GameJam Shift 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "DimensionShiftCameraShake.generated.h"

/**
 * Custom camera shake pattern for the Dimension Shift transition:
 * - Duration: 0.2s
 * - Pitch oscillation amplitude: 5.0 deg
 * - FOV kick: 10.0 deg
 */
UCLASS(EditInlineNew)
class GAMEJAM_SHIFT_2026_API UDimensionShiftCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UDimensionShiftCameraShakePattern(const FObjectInitializer& ObjectInitializer);

	/** Total duration of the dimension shift shake in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift Shake", meta = (ClampMin = "0.05"))
	float Duration = 0.2f;

	/** Pitch oscillation amplitude in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift Shake", meta = (ClampMin = "0.0"))
	float PitchAmplitude = 5.0f;

	/** Pitch oscillation frequency in radians/sec. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift Shake", meta = (ClampMin = "1.0"))
	float PitchFrequency = 45.0f;

	/** Peak FOV punch in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimension Shift Shake", meta = (ClampMin = "0.0"))
	float FOVKick = 10.0f;

protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

private:
	float ElapsedTime = 0.0f;
	bool bIsFinished = false;
};

/**
 * Camera shake base class for Dimension Shift transitions (parent of CS_DimensionShift).
 */
UCLASS(Blueprintable)
class GAMEJAM_SHIFT_2026_API UDimensionShiftCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UDimensionShiftCameraShake(const FObjectInitializer& ObjectInitializer);
};
