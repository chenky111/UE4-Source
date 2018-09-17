// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraVolumeRendererProperties.h"
#include "NiagaraRenderer.h"
#include "Internationalization/Internationalization.h"
#include "NiagaraRendererVolumes.h"
#include "NiagaraConstants.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "UNiagaraVolumeRendererProperties"

UNiagaraVolumeRendererProperties::UNiagaraVolumeRendererProperties()
	: SyncId(0)
{
	{
#if WITH_EDITORONLY_DATA
		if (!IsRunningCommandlet())
		{
			// Structure to hold one-time initialization
			struct FConstructorStatics
			{
				ConstructorHelpers::FObjectFinderOptional<UMaterial> MaterialObject;
				ConstructorHelpers::FObjectFinderOptional<UStaticMesh> MeshObject;
				FConstructorStatics()
					: MaterialObject(TEXT("/Niagara/VolumeTextures/Materials/M_VolumeRayMarch_Lit_LinearSteps_Preview.M_VolumeRayMarch_Lit_LinearSteps_Preview")),
					MeshObject(TEXT("/Niagara/RayMarching/Meshes/S_Box_InsideOut_01.S_Box_InsideOut_01"))
				{
				}
			};
			static FConstructorStatics ConstructorStatics;

			ParticleMesh = ConstructorStatics.MeshObject.Get();
			Material = ConstructorStatics.MaterialObject.Get();
		}
#endif
	}
}

NiagaraRenderer* UNiagaraVolumeRendererProperties::CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel)
{
	return new NiagaraRendererVolumes(FeatureLevel, this);
}

void UNiagaraVolumeRendererProperties::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	OutMaterials.Add(Material);
}

void UNiagaraVolumeRendererProperties::PostInitProperties()
{
	Super::PostInitProperties();
	SyncId = 0;
}

/** The bindings depend on variables that are created during the NiagaraModule startup. However, the CDO's are build prior to this being initialized, so we defer setting these values until later.*/
void UNiagaraVolumeRendererProperties::InitCDOPropertiesAfterModuleStartup()
{
	UNiagaraVolumeRendererProperties* CDO = CastChecked<UNiagaraVolumeRendererProperties>(UNiagaraVolumeRendererProperties::StaticClass()->GetDefaultObject());
	CDO->InitBindings();
}

void UNiagaraVolumeRendererProperties::InitBindings()
{
}


#if WITH_EDITORONLY_DATA
void UNiagaraVolumeRendererProperties::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName() != TEXT("SyncId"))
	{
		SyncId++;
	}
}


const TArray<FNiagaraVariable>& UNiagaraVolumeRendererProperties::GetRequiredAttributes()
{
	static TArray<FNiagaraVariable> Attrs;
	return Attrs;
}


const TArray<FNiagaraVariable>& UNiagaraVolumeRendererProperties::GetOptionalAttributes()
{
	static TArray<FNiagaraVariable> Attrs;
	return Attrs;
}


bool UNiagaraVolumeRendererProperties::IsMaterialValidForRenderer(UMaterial* InMaterial, FText& InvalidMessage)
{
	return true;
}

void UNiagaraVolumeRendererProperties::FixMaterial(UMaterial* InMaterial)
{
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITORONLY_DATA




