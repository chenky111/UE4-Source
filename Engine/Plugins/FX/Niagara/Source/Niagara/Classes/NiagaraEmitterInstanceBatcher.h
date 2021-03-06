// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEmitterInstanceBatcher.h: Queueing and batching for Niagara simulation;
use to reduce per-simulation overhead by batching together simulations using
the same VectorVM byte code / compute shader code
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "RendererInterface.h"
#include "NiagaraParameters.h"
#include "NiagaraEmitter.h"
#include "Tickable.h"
#include "Modules/ModuleManager.h"
#include "RHIResources.h"
#include "FXSystem.h"
#include "NiagaraRendererProperties.h"
#include "ParticleResources.h"
#include "Runtime/Engine/Private/Particles/ParticleSortingGPU.h"
#include "NiagaraGPUSortInfo.h"

struct FNiagaraScriptExecutionContext;
struct FNiagaraComputeExecutionContext;

#define SIMULATION_QUEUE_COUNT 2

class FNiagaraIndicesVertexBuffer : public FParticleIndicesVertexBuffer
{
public:

	FNiagaraIndicesVertexBuffer(int32 InIndexCount);

	FUnorderedAccessViewRHIRef VertexBufferUAV;

	// The allocation count.
	const int32 IndexCount;

	// Currently used count.
	int32 UsedIndexCount = 0;
};

class NiagaraEmitterInstanceBatcher : public FFXSystemInterface
{
public:

	NiagaraEmitterInstanceBatcher(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform)
		: FeatureLevel(InFeatureLevel)
		, ShaderPlatform(InShaderPlatform)
		, CurQueueIndex(0)
		, ParticleSortBuffers(true)
	{
	}

	~NiagaraEmitterInstanceBatcher();

	static const FName Name;
	virtual FFXSystemInterface* GetInterface(const FName& InName) override;

	void Queue(FNiagaraComputeExecutionContext *InContext);

	void Remove(FNiagaraComputeExecutionContext* InContext);

#if WITH_EDITOR
	virtual void Suspend() override {}
	virtual void Resume() override {}
#endif // #if WITH_EDITOR

	virtual void DrawDebug(FCanvas* Canvas) override {}
	virtual void AddVectorField(UVectorFieldComponent* VectorFieldComponent) override {}
	virtual void RemoveVectorField(UVectorFieldComponent* VectorFieldComponent) override {}
	virtual void UpdateVectorField(UVectorFieldComponent* VectorFieldComponent) override {}
	virtual void PreInitViews() override;
	virtual bool UsesGlobalDistanceField() const override { return false; }
	virtual void PreRender(FRHICommandListImmediate& RHICmdList, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData) override {}

	virtual void Tick(float DeltaTime) override
	{
		BuildBatches();
	}

	// TODO: process queue, build batches from context with the same script
	//  also need to figure out how to handle multiple sets of parameters across a batch
	//	for now this executes every single sim in the queue individually, which is terrible 
	//	in terms of overhead
	void BuildBatches()
	{
	}

	uint32 GetEventSpawnTotal(const FNiagaraComputeExecutionContext *InContext) const;

	virtual void PostRenderOpaque(
		FRHICommandListImmediate& RHICmdList,
		const FUniformBufferRHIParamRef ViewUniformBuffer,
		const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct,
		FUniformBufferRHIParamRef SceneTexturesUniformBuffer) override
	{
		CurQueueIndex ^= 0x1;

		ExecuteAll(RHICmdList, ViewUniformBuffer);
		SortGPUParticles(RHICmdList);
	}

	void ExecuteAll(FRHICommandList &RHICmdList, FUniformBufferRHIParamRef ViewUniformBuffer);
	void TickSingle(FNiagaraComputeExecutionContext *Context, FRHICommandList &RHICmdList, FUniformBufferRHIParamRef ViewUniformBuffer) const;

	int32 AddSortedGPUSimulation(const FNiagaraGPUSortInfo& SortInfo);
	void SortGPUParticles(FRHICommandListImmediate& RHICmdList);
	void ResolveParticleSortBuffers(FRHICommandListImmediate& RHICmdList, int32 ResultBufferIndex);

	const FParticleIndicesVertexBuffer& GetGPUSortedBuffer() const { return SortedVertexBuffers.Last(); }

	void ProcessDebugInfo(FRHICommandList &RHICmdList, const FNiagaraComputeExecutionContext *Context) const;

	void SetPrevDataStrideParams(const FNiagaraDataSet *Set, FNiagaraShader *Shader, FRHICommandList &RHICmdList) const;

	void SetupEventUAVs(const FNiagaraComputeExecutionContext *Context, uint32 NumInstances, FRHICommandList &RHICmdList) const;
	void UnsetEventUAVs(const FNiagaraComputeExecutionContext *Context, FRHICommandList &RHICmdList) const;
	void SetDataInterfaceParameters(const TArray<UNiagaraDataInterface*> &DataInterfaces, FNiagaraShader *Shader, FRHICommandList &RHICmdList, const FNiagaraComputeExecutionContext *Context) const;
	void UnsetDataInterfaceParameters(const TArray<UNiagaraDataInterface*> &DataInterfaces, FNiagaraShader* Shader, FRHICommandList &RHICmdList, const FNiagaraComputeExecutionContext *Context) const;

	void Run(	const FNiagaraComputeExecutionContext *Context, 
				uint32 UpdateStartInstance, 
				const uint32 TotalNumInstances, 
				FNiagaraShader* Shader,
				FRHICommandList &RHICmdList, 
				FUniformBufferRHIParamRef ViewUniformBuffer, 
				bool bCopyBeforeStart = false
			) const;

	void RunEventHandlers(const FNiagaraComputeExecutionContext *Context, uint32 NumInstancesAfterSim, uint32 NumInstancesAfterSpawn, uint32 NumInstancesAfterNonEventSpawn, FRHICommandList &RhiCmdList) const;

	void ClearIndexBufferCur(FRHICommandList &RHICmdList, FNiagaraComputeExecutionContext *Context) const;
	void ResolveDatasetWrites(FRHICommandList &RHICmdList, FNiagaraComputeExecutionContext *Context) const;
	void ResizeCurrentBuffer(FRHICommandList &RHICmdList, FNiagaraComputeExecutionContext *Context, uint32 NewNumInstances, uint32 PrevNumInstances) const;

private:

	/** Feature level of this effects system */
	ERHIFeatureLevel::Type FeatureLevel;
	/** Shader platform that will be rendering this effects system */
	EShaderPlatform ShaderPlatform;

	uint32 CurQueueIndex;

	TArray<FNiagaraComputeExecutionContext*> SimulationQueue[SIMULATION_QUEUE_COUNT];

	// Number of particle to sort this frame.
	int32 SortedParticleCount = 0;
	int32 NumFramesRequiringShrinking = 0;
	TArray<FNiagaraGPUSortInfo> SimulationsToSort;
	FParticleSortBuffers ParticleSortBuffers;

	// The result of the GPU sort. Each next element replace the previous.
	// The last entry is used to transfer the result of the ParticleSortBuffers.
	TIndirectArray<FNiagaraIndicesVertexBuffer> SortedVertexBuffers;
};
