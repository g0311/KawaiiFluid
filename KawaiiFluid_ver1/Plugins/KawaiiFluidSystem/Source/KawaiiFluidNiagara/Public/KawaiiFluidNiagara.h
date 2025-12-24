// Copyright KawaiiFluid Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Kawaii Fluid Niagara 통합 모듈
 * CPU 시뮬레이션 데이터를 Niagara로 전달하는 Data Interface 제공
 */
class FKawaiiFluidNiagaraModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
