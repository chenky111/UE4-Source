// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "NiagaraVolumeRendererProperties.h"
#include "NiagaraRenderer.h"

class FNiagaraDataSet;



struct FNiagaraDynamicDataVolumes : public FNiagaraDynamicDataBase
{
	TMap<FName, UTexture*> TextureArray;
};

/**
* NiagaraRendererSprites renders an FNiagaraEmitterInstance as sprite particles
*/
class NIAGARA_API NiagaraRendererVolumes : public NiagaraRenderer
{
public:

	explicit NiagaraRendererVolumes(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *Props);
	~NiagaraRendererVolumes()
	{
		ReleaseRenderThreadResources();
	}


	virtual void ReleaseRenderThreadResources() override;

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual bool SetMaterialUsage() override;
	virtual void TransformChanged() override;
	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) override;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)
	{
		FPrimitiveViewRelevance Result;
		bool bHasDynamicData = HasDynamicData();
		Result.bDrawRelevance = bHasDynamicData && SceneProxy->IsShown(View) && View->Family->EngineShowFlags.Particles;
		Result.bShadowRelevance = bHasDynamicData && SceneProxy->IsShadowCast(View);
		Result.bDynamicRelevance = bHasDynamicData;

		if (bHasDynamicData)
		{
			Result.bOpaqueRelevance = MaterialRelevance.bOpaque;
			Result.bNormalTranslucencyRelevance = MaterialRelevance.bNormalTranslucency;
			Result.bSeparateTranslucencyRelevance = MaterialRelevance.bSeparateTranslucency;
			Result.bDistortionRelevance = MaterialRelevance.bDistortion;
		}

		return Result;
	}




	UClass *GetPropertiesClass() override { return UNiagaraVolumeRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraRendererProperties *Props) override { Properties = Cast<UNiagaraVolumeRendererProperties>(Props); }
	virtual UNiagaraRendererProperties* GetRendererProperties()  const override {
		return Properties;
	}
#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif

private:
	UNiagaraVolumeRendererProperties *Properties;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;

	int32 LastSyncedId;
};
