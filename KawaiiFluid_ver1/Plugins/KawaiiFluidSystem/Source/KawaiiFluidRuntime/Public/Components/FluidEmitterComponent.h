// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FluidEmitterComponent.generated.h"

class AFluidSimulator;

UENUM(BlueprintType)
enum class EEmitterPattern : uint8
{
	Point      UMETA(DisplayName = "Point"),
	Sphere     UMETA(DisplayName = "Sphere"),
	Cone       UMETA(DisplayName = "Cone"),
	Box        UMETA(DisplayName = "Box")
};

/**
 * 유체 발생기 컴포넌트
 */
UCLASS(ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent))
class KAWAIIFLUIDRUNTIME_API UFluidEmitterComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFluidEmitterComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter")
	AFluidSimulator* TargetSimulator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter")
	bool bIsEmitting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter")
	EEmitterPattern Pattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter", meta = (ClampMin = "1", ClampMax = "1000"))
	float ParticlesPerSecond;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter", meta = (ClampMin = "0.1"))
	float EmitRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter")
	FVector EmitBoxExtent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float ConeAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter")
	FVector InitialVelocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter")
	float VelocityRandomness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter|Burst")
	bool bBurstMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Emitter|Burst", meta = (EditCondition = "bBurstMode"))
	int32 BurstCount;

	UFUNCTION(BlueprintCallable, Category = "Fluid Emitter")
	void StartEmitting();

	UFUNCTION(BlueprintCallable, Category = "Fluid Emitter")
	void StopEmitting();

	UFUNCTION(BlueprintCallable, Category = "Fluid Emitter")
	void Burst(int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Fluid Emitter")
	void SetTargetSimulator(AFluidSimulator* Simulator);

protected:
	virtual void BeginPlay() override;

private:
	float AccumulatedTime;

	FVector CalculateEmitPosition() const;
	FVector CalculateEmitVelocity() const;
	void FindSimulatorInWorld();
};
