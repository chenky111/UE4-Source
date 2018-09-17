// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraRendererProperties.h"
#include "StaticMeshResources.h"
#include "NiagaraVolumeRendererProperties.generated.h"




UCLASS(editinlinenew)
class NIAGARA_API UNiagaraVolumeRendererProperties : public UNiagaraRendererProperties
{
public:
	GENERATED_BODY()

	UNiagaraVolumeRendererProperties();

	virtual void PostInitProperties() override;

	static void InitCDOPropertiesAfterModuleStartup();

	//~ UNiagaraRendererProperties interface
	virtual NiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual bool IsSimTargetSupported(ENiagaraSimTarget InSimTarget) const override { return true; };
#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) override;
	virtual void FixMaterial(UMaterial* Material) override;
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif // WITH_EDITORONLY_DATA

	// TODO: once we support cutouts, need to change this
	virtual uint32 GetNumIndicesPerInstance() { return 0; }

	/** The material used to render the particle. Note that it must have the Use with Niagara Volumes flag checked.*/
	UPROPERTY(EditAnywhere, Category = "Volume Rendering")
	UMaterialInterface* Material;

	/** The static mesh to be used as the proxy shape to render for the material.*/
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering")
	UStaticMesh *ParticleMesh;
	
	/** Imagine the particle texture having an arrow pointing up, these modes define how the particle aligns that texture to other particle attributes.*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	TMap<FName, FNiagaraVariableDataInterfaceBinding> MaterialBindings;

	UPROPERTY(Transient)
	int32 SyncId;

	UPROPERTY(EditAnywhere, Category = "Velocity Settings", meta = (DisplayName = "Offset"))
	FVector VelocityOffset;

	void InitBindings();
};



