// Copyright GameJam Shift 2026. All Rights Reserved.

#include "DimensionShiftCameraShake.h"

UDimensionShiftCameraShakePattern::UDimensionShiftCameraShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Duration = 0.2f;
	PitchAmplitude = 5.0f;
	PitchFrequency = 45.0f;
	FOVKick = 10.0f;
}

void UDimensionShiftCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void UDimensionShiftCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	ElapsedTime = 0.0f;
	bIsFinished = false;
}

void UDimensionShiftCameraShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	if (bIsFinished)
	{
		return;
	}

	ElapsedTime += Params.DeltaTime;
	if (Duration <= KINDA_SMALL_NUMBER || ElapsedTime >= Duration)
	{
		bIsFinished = true;
		OutResult.Rotation = FRotator::ZeroRotator;
		OutResult.FOV = 0.0f;
		return;
	}

	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	const float Envelope = 1.0f - Alpha;

	// Damped pitch oscillation (5.0 deg peak) + sharp FOV kick (10.0 deg peak)
	const float PitchOffset = PitchAmplitude * FMath::Sin(ElapsedTime * PitchFrequency) * Envelope;
	const float FOVOffset = FOVKick * FMath::Sin(Alpha * PI);

	OutResult.Rotation = FRotator(PitchOffset, 0.0f, 0.0f);
	OutResult.FOV = FOVOffset;
}

bool UDimensionShiftCameraShakePattern::IsFinishedImpl() const
{
	return bIsFinished;
}

UDimensionShiftCameraShake::UDimensionShiftCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(CreateDefaultSubobject<UDimensionShiftCameraShakePattern>(TEXT("DimensionShiftShakePattern")));
}

UHitCameraShakePattern::UHitCameraShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHitCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void UHitCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	ElapsedTime = 0.0f;
	bIsFinished = false;
}

void UHitCameraShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	if (bIsFinished)
	{
		return;
	}

	ElapsedTime += Params.DeltaTime;
	if (Duration <= KINDA_SMALL_NUMBER || ElapsedTime >= Duration)
	{
		bIsFinished = true;
		OutResult.Rotation = FRotator::ZeroRotator;
		OutResult.Location = FVector::ZeroVector;
		OutResult.FOV = 0.0f;
		return;
	}

	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	const float Decay = FMath::Pow(1.0f - Alpha, 1.8f);

	// Smooth rotational impact jolt (zero location/FOV change so it never feels like a teleport)
	const float Pitch = PitchAmplitude * FMath::Sin(ElapsedTime * 52.0f) * Decay;
	const float Yaw = YawAmplitude * FMath::Sin(ElapsedTime * 41.0f + 0.9f) * Decay;
	const float Roll = RollAmplitude * FMath::Sin(ElapsedTime * 48.0f + 0.4f) * Decay;

	OutResult.Rotation = FRotator(Pitch, Yaw, Roll);
	OutResult.Location = FVector::ZeroVector;
	OutResult.FOV = 0.0f;
}

bool UHitCameraShakePattern::IsFinishedImpl() const
{
	return bIsFinished;
}

UHitCameraShake::UHitCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UHitCameraShakePattern>(TEXT("HitShakePattern")));
}
