// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FluidInteractionComponent.generated.h"

class AFluidSimulator;
class UFluidCollider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFluidAttached, int32, ParticleCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFluidDetached);

/**
 * 유체 상호작용 컴포넌트
 * 캐릭터/오브젝트에 붙여서 유체와 상호작용
 */
UCLASS(ClassGroup=(KawaiiFluid), meta=(BlueprintSpawnableComponent))
class KAWAIIFLUIDRUNTIME_API UFluidInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFluidInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Interaction")
	AFluidSimulator* TargetSimulator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Interaction")
	bool bCanAttachFluid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Interaction", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float AdhesionMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Interaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DragAlongStrength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fluid Interaction")
	bool bAutoCreateCollider;

	UPROPERTY(BlueprintReadOnly, Category = "Fluid Interaction|Status")
	int32 AttachedParticleCount;

	UPROPERTY(BlueprintReadOnly, Category = "Fluid Interaction|Status")
	bool bIsWet;

	UPROPERTY(BlueprintAssignable, Category = "Fluid Interaction|Events")
	FOnFluidAttached OnFluidAttached;

	UPROPERTY(BlueprintAssignable, Category = "Fluid Interaction|Events")
	FOnFluidDetached OnFluidDetached;

	UFUNCTION(BlueprintCallable, Category = "Fluid Interaction")
	int32 GetAttachedParticleCount() const { return AttachedParticleCount; }

	UFUNCTION(BlueprintCallable, Category = "Fluid Interaction")
	void DetachAllFluid();

	UFUNCTION(BlueprintCallable, Category = "Fluid Interaction")
	void PushFluid(FVector Direction, float Force);

	UFUNCTION(BlueprintCallable, Category = "Fluid Interaction")
	void SetTargetSimulator(AFluidSimulator* Simulator);

	UFUNCTION(BlueprintCallable, Category = "Fluid Interaction")
	bool IsWet() const { return bIsWet; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	UFluidCollider* AutoCollider;

	void CreateAutoCollider();
	void RegisterWithSimulator();
	void UnregisterFromSimulator();
	void UpdateAttachedParticleCount();
};
