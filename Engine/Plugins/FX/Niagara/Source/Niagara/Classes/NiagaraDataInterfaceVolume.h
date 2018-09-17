// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/StrongObjectPtr.h"
#include "NiagaraDataInterfaceVolume.generated.h"

/** Defines modes for updating the component's age. */
UENUM()
enum class ENiagaraVolumeDimensions : uint8
{
	Volume_32x32x32 = 0,
	Volume_64x64x64,
	Volume_100x100x100,
	Volume_128x128x128,
	Volume_196x196x196,
	Volume_256x256x256,
	Volume_324x324x324,
	Volume_400x400x400 /* Beware!*/
};


struct NIAGARA_API FNDIVolume_InstanceData
{
	FNDIVolume_InstanceData();
	~FNDIVolume_InstanceData();

	bool bSwap;
	TStrongObjectPtr<UTexture> VolumeTexA;
	TStrongObjectPtr<UTexture> VolumeTexB;
	FUnorderedAccessViewRHIRef VolTexAUAV;
	FUnorderedAccessViewRHIRef VolTexBUAV;
	TAtomic<bool> bSetInRT;

	bool Init(class UNiagaraDataInterfaceVolume* Volume, FNiagaraSystemInstance* SystemInstance);
	void CreateRenderTarget(int32 InWidth, int32 InHeight, ETextureRenderTargetFormat InFormat, int32 InIdx);
	bool SyncrhonizeSizesWithVolume(class UNiagaraDataInterfaceVolume* Volume);
	UTextureRenderTarget2D* GetReadTexture();
	FTextureRHIParamRef GetReadTextureSRV();
	FUnorderedAccessViewRHIParamRef GetReadTextureUAV();

	void CreateUAVs();

	UTextureRenderTarget2D* GetWriteTexture();
	FTextureRHIParamRef GetWriteTextureSRV();
	FUnorderedAccessViewRHIParamRef GetWriteTextureUAV();
};

/** Data Interface allowing sampling of a texture */
UCLASS(EditInlineNew, Category = "Volumetrics", meta = (DisplayName = "3D Volume"))
class NIAGARA_API UNiagaraDataInterfaceVolume : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Texture")
	ENiagaraVolumeDimensions Dimensions;

	UPROPERTY()
	FVector2D XYFrames;

	UPROPERTY()
	FVector2D Resolution2D;

	UPROPERTY(EditAnywhere, Category = "Velocity Settings", meta = (DisplayName = "Offset"))
	FVector VelocityOffset;


	void SetDimensions(ENiagaraVolumeDimensions Dims);

	//UObject Interface
	virtual void PostInitProperties()override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return Target==ENiagaraSimTarget::GPUComputeSim; }

	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual int32 PerInstanceDataSize()const override { return sizeof(FNDIVolume_InstanceData); }
	//UNiagaraDataInterface Interface End

	void SampleVolumeTexture(FVectorVMContext& Context);

	void WriteToVolumeTexture(FVectorVMContext& Context);

	void ReadFromClosestVolumeCellTexture(FVectorVMContext& Context);

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// GPU sim functionality
	virtual bool GetFunctionHLSL(const FName&  DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters()const override;

	//FRWBuffer& GetGPUBuffer();
	static const FString TextureName;
	static const FString SamplerName;


protected:

	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	static const FName SampleVolumeName;
	static const FName WriteToVolumeName;
	static const FName ReadFromClosestVolumeCellName;
};
