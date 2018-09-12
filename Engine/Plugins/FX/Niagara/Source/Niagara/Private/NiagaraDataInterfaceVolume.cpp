// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceVolume.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
#include "Kismet/KismetRenderingLibrary.h"

#define LOCTEXT_NAMESPACE "UNiagaraDataInterfaceVolume"

const FName UNiagaraDataInterfaceVolume::SampleVolumeName(TEXT("SampleVolume"));
const FName UNiagaraDataInterfaceVolume::ReadFromClosestVolumeCellName(TEXT("ReadFromClosestVolumeCell"));
const FName UNiagaraDataInterfaceVolume::WriteToVolumeName(TEXT("WriteToVolume"));
const FString UNiagaraDataInterfaceVolume::TextureName(TEXT("Texture_"));
const FString UNiagaraDataInterfaceVolume::SamplerName(TEXT("Sampler_"));

UNiagaraDataInterfaceVolume::UNiagaraDataInterfaceVolume(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Dimensions(ENiagaraVolumeDimensions::Volume_256x256x256)
	, XYFrames(0.0f, 0.0f)
	, Resolution2D(0.0f, 0.0f)
{
	SetDimensions(Dimensions);
}

void UNiagaraDataInterfaceVolume::SetDimensions(ENiagaraVolumeDimensions InDims)
{
	switch (InDims)
	{
	default:
	case ENiagaraVolumeDimensions::Volume_32x32x32:
		Dimensions = ENiagaraVolumeDimensions::Volume_32x32x32;
		XYFrames = FVector2D(32.0f, 1.0f);
		Resolution2D = FVector2D(1024.0f, 32.0f);
		break;
	case ENiagaraVolumeDimensions::Volume_64x64x64:
		Dimensions = InDims;
		XYFrames = FVector2D(8.0f, 8.0f);
		Resolution2D = FVector2D(512.0f, 512.0f);
		break;
	case ENiagaraVolumeDimensions::Volume_100x100x100:
		Dimensions = InDims;
		XYFrames = FVector2D(10.0f, 10.0f);
		Resolution2D = FVector2D(1000.0f, 1000.0f);
		break;
	case ENiagaraVolumeDimensions::Volume_128x128x128:
		Dimensions = InDims;
		XYFrames = FVector2D(16.0f, 8.0f);
		Resolution2D = FVector2D(2048.0f, 1024.0f);
		break;
	case ENiagaraVolumeDimensions::Volume_196x196x196:
		Dimensions = InDims;
		XYFrames = FVector2D(14.0f, 14.0f);
		Resolution2D = FVector2D(2744.0f, 2744.0f);
		break;
	case ENiagaraVolumeDimensions::Volume_256x256x256:
		Dimensions = InDims;
		XYFrames = FVector2D(16.0f, 16.0f);
		Resolution2D = FVector2D(4096.0f, 4096.0f);
		break;
	case ENiagaraVolumeDimensions::Volume_324x324x324:
		Dimensions = InDims;
		XYFrames = FVector2D(18.0f, 18.0f);
		Resolution2D = FVector2D(5184.0f, 5184.0f);
		break;
	case ENiagaraVolumeDimensions::Volume_400x400x400:
		Dimensions = InDims;
		XYFrames = FVector2D(20.0f, 20.0f);
		Resolution2D = FVector2D(8000.0f, 8000.0f);
		break;
	}

}

void UNiagaraDataInterfaceVolume::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}
}

void UNiagaraDataInterfaceVolume::PostLoad()
{
	Super::PostLoad();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceVolume::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceVolume, Dimensions))
	{
		SetDimensions(Dimensions);
	}
}

#endif

bool UNiagaraDataInterfaceVolume::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}
	UNiagaraDataInterfaceVolume* DestinationTexture = CastChecked<UNiagaraDataInterfaceVolume>(Destination);
	DestinationTexture->Dimensions = Dimensions;
	DestinationTexture->XYFrames = XYFrames;
	DestinationTexture->Resolution2D = Resolution2D;

	return true;
}


bool UNiagaraDataInterfaceVolume::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceVolume* OtherTexture = CastChecked<const UNiagaraDataInterfaceVolume>(Other);
	return OtherTexture->Dimensions == Dimensions &&
		OtherTexture->XYFrames == XYFrames  && OtherTexture->Resolution2D == Resolution2D;
}

bool UNiagaraDataInterfaceVolume::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIVolume_InstanceData* Inst = new (PerInstanceData) FNDIVolume_InstanceData();
	check(IsAligned(PerInstanceData, 16));
	return Inst->Init(this, SystemInstance);
}

void UNiagaraDataInterfaceVolume::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIVolume_InstanceData* Inst = (FNDIVolume_InstanceData*)PerInstanceData;
	Inst->~FNDIVolume_InstanceData();
}

bool UNiagaraDataInterfaceVolume::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	return false;
}

bool FNDIVolume_InstanceData::Init(UNiagaraDataInterfaceVolume* Volume, FNiagaraSystemInstance* SystemInstance)
{
	SyncrhonizeSizesWithVolume(Volume);
	return true;
}

FNDIVolume_InstanceData::FNDIVolume_InstanceData() :
	bSwap(false)
	, VolumeTexA(nullptr)
	, VolumeTexB(nullptr)
	, VolTexAUAV(nullptr)
	, VolTexBUAV(nullptr)
	, bSetInRT(false)
{

}

FNDIVolume_InstanceData::~FNDIVolume_InstanceData()
{

}

UTextureRenderTarget2D* FNDIVolume_InstanceData::GetReadTexture()
{
	check(IsInRenderingThread());
	UTexture* ReadTexture = bSwap ? VolumeTexB.Get() : VolumeTexA.Get();
	return Cast<UTextureRenderTarget2D>(ReadTexture);
}

UTextureRenderTarget2D* FNDIVolume_InstanceData::GetWriteTexture()
{
	check(IsInRenderingThread());
	UTexture* WriteTexture = bSwap ? VolumeTexA.Get() : VolumeTexB.Get();
	return Cast<UTextureRenderTarget2D>(WriteTexture);
}

FTextureRHIParamRef FNDIVolume_InstanceData::GetReadTextureSRV()
{
	check(IsInRenderingThread());
	FTextureRHIParamRef Ref = nullptr;
	UTexture* ReadTex = GetReadTexture();
	if (ReadTex && ReadTex->Resource)
	{
		Ref = ReadTex->Resource->TextureRHI;
	}
	return Ref;

}

FTextureRHIParamRef FNDIVolume_InstanceData::GetWriteTextureSRV()
{
	check(IsInRenderingThread());
	FTextureRHIParamRef Ref = nullptr;
	UTexture* WriteTex = GetWriteTexture();
	if (WriteTex && WriteTex->Resource)
	{
		Ref = WriteTex->Resource->TextureRHI;
	}
	return Ref;

}
FUnorderedAccessViewRHIParamRef FNDIVolume_InstanceData::GetReadTextureUAV()
{
	check(IsInRenderingThread());
	FUnorderedAccessViewRHIParamRef ReadTextureRef = bSwap ? VolTexBUAV : VolTexAUAV;
	return ReadTextureRef;
}

FUnorderedAccessViewRHIParamRef FNDIVolume_InstanceData::GetWriteTextureUAV()
{
	check(IsInRenderingThread());
	FUnorderedAccessViewRHIParamRef WriteTextureRef = bSwap ? VolTexAUAV : VolTexBUAV;
	return WriteTextureRef;
}



bool FNDIVolume_InstanceData::SyncrhonizeSizesWithVolume(UNiagaraDataInterfaceVolume* Volume)
{
	check(!bSetInRT);

	if (VolumeTexA.IsValid() == false || VolumeTexB.IsValid() == false ||
		((VolumeTexA->GetSurfaceHeight()*VolumeTexA->GetSurfaceWidth()) != Volume->Resolution2D.X * Volume->Resolution2D.Y) ||
		((VolumeTexB->GetSurfaceHeight()*VolumeTexB->GetSurfaceWidth()) != Volume->Resolution2D.X * Volume->Resolution2D.Y))
	{
		VolumeTexA.Reset();
		VolumeTexB.Reset();
	}

	if (VolumeTexA.IsValid() == false || VolumeTexB.IsValid() == false ||
		((VolumeTexA->GetSurfaceHeight()*VolumeTexA->GetSurfaceWidth()) != Volume->Resolution2D.X * Volume->Resolution2D.Y) ||
		((VolumeTexB->GetSurfaceHeight()*VolumeTexB->GetSurfaceWidth()) != Volume->Resolution2D.X * Volume->Resolution2D.Y))
	{
		uint32 RealWidth = (uint32)Volume->Resolution2D.X;
		uint32 RealHeight = (uint32)Volume->Resolution2D.Y;
		uint32 NumPixels = RealWidth * RealHeight;
		if (NumPixels != 0)
		{
			// Create the rendered textures here with the appropriate dimensions and read flags
			CreateRenderTarget(RealWidth, RealHeight, RTF_RGBA16f, 0);
			CreateRenderTarget(RealWidth, RealHeight, RTF_RGBA16f, 1);
			return true;
		}
	}

	return false;
}

void FNDIVolume_InstanceData::CreateRenderTarget(int32 InWidth, int32 InHeight, ETextureRenderTargetFormat InFormat, int32 InIdx)
{
	check(!bSetInRT);

	/*
	uint32 TexCreateFlags = 0;
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		TexCreateFlags = TexCreate_ShaderResource | TexCreate_UAV;
	}

	FRHIResourceCreateInfo CreateInfo;
	FTexture2DRHIRef ShaderResource = RHICreateTexture2D(InWidth, InHeight, InFormat, 1, 1, TexCreateFlags, CreateInfo);

	FUnorderedAccessViewRHIRef VolumeTextureUAV;
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		VolumeTextureUAV = RHICreateUnorderedAccessView(ShaderResource);
	}
	*/

	//UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);

	UTextureRenderTarget2D* NewRenderTarget2D = nullptr;
	if (InWidth > 0 && InHeight > 0 && FApp::CanEverRender())
	{
		NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		check(NewRenderTarget2D);
		NewRenderTarget2D->RenderTargetFormat = InFormat;
		NewRenderTarget2D->bCanCreateUAV = true;
		NewRenderTarget2D->InitAutoFormat(InWidth, InHeight);
		NewRenderTarget2D->ClearColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
		NewRenderTarget2D->UpdateResourceImmediate(true);
	}

	if (InIdx == 0)
	{
		VolumeTexA.Reset(NewRenderTarget2D);
	}
	else if (InIdx == 1)
	{
		VolumeTexB.Reset(NewRenderTarget2D);
	}
}

void FNDIVolume_InstanceData::CreateUAVs()
{
	check(!bSetInRT);

	if (VolumeTexA.IsValid() && VolTexAUAV.IsValid() == false)
	{		
		if (VolumeTexA->Resource && VolumeTexA->Resource->TextureRHI)
		{
			VolTexAUAV = RHICreateUnorderedAccessView(VolumeTexA->Resource->TextureRHI);
			if (VolTexAUAV.IsValid() == false)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Failed to create UAV for VolTexA!"))
			}
		}
		else
		{
			VolTexAUAV = FUnorderedAccessViewRHIRef();
		}
	}
	
	if (VolumeTexB.IsValid() && VolTexBUAV.IsValid() == false)
	{
	
		if (VolumeTexB->Resource && VolumeTexB->Resource->TextureRHI)
		{
			VolTexBUAV = RHICreateUnorderedAccessView(VolumeTexB->Resource->TextureRHI);
			if (VolTexBUAV.IsValid() == false)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Failed to create UAV for VolTexB! "))
			}
		}
		else
		{
			VolTexBUAV = FUnorderedAccessViewRHIRef();
		}
	}
}

void UNiagaraDataInterfaceVolume::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = SampleVolumeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Volume")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("UVW")));
		Sig.SetDescription(LOCTEXT("TextureSampleDesc", "Sample the volume at the specified UVW coordinates. The UVW origin (0,0,0) is in the bottom left hand corner of the volume."));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));
		//Sig.Owner = *GetName();

		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = WriteToVolumeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Volume")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("UVW")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));
		Sig.SetDescription(LOCTEXT("TextureSampleDesc", "Write to the volume at the specified UVW coordinates. The UVW origin (0,0,0) is in the bottom left hand corner of the volume."));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));
		//Sig.Owner = *GetName();

		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = ReadFromClosestVolumeCellName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Volume")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("UVW")));
		Sig.SetDescription(LOCTEXT("TextureSampleDesc", "Read from the volume at the closest cell specified by the UVW coordinates. The UVW origin (0,0,0) is in the bottom left hand corner of the volume."));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));
		//Sig.Owner = *GetName();

		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceVolume, SampleVolumeTexture);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceVolume, WriteToVolumeTexture);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceVolume, ReadFromClosestVolumeCellTexture);
void UNiagaraDataInterfaceVolume::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	// Whatever are we going to do on CPU?
	// TODO sckime

	if (BindingInfo.Name == SampleVolumeName)
	{
		//check(BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceVolume, SampleVolumeTexture)::Bind(this, OutFunc);
	}
	else if(BindingInfo.Name == WriteToVolumeName)
	{
		//check(BindingInfo.GetNumInputs() == 6 && BindingInfo.GetNumOutputs() == 3);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceVolume, WriteToVolumeTexture)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == ReadFromClosestVolumeCellName)
	{
		//check(BindingInfo.GetNumInputs() == 6 && BindingInfo.GetNumOutputs() == 3);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceVolume, ReadFromClosestVolumeCellTexture)::Bind(this, OutFunc);
	}


}


void UNiagaraDataInterfaceVolume::WriteToVolumeTexture(FVectorVMContext& Context)
{
	// Do we even want this func?
}

void UNiagaraDataInterfaceVolume::ReadFromClosestVolumeCellTexture(FVectorVMContext& Context)
{
	// Do we even want this func?
}

void UNiagaraDataInterfaceVolume::SampleVolumeTexture(FVectorVMContext& Context)
{
	// Do we even want this func?

	VectorVM::FExternalFuncInputHandler<float> XParam(Context);
	VectorVM::FExternalFuncInputHandler<float> YParam(Context);
	//ZType ZParam(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleR(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleG(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleB(Context);

	//if (CPUTextureData.GetAllocatedSize() == 0 /*|| Texture == nullptr*/)
	//{
	//	for (int32 i = 0; i < Context.NumInstances; ++i)
	//	{
	//		float X = XParam.GetAndAdvance();
	//		float Y = YParam.GetAndAdvance();
	//		*OutSampleR.GetDestAndAdvance() = 1.0;
	//		*OutSampleG.GetDestAndAdvance() = 0.0;
	//		*OutSampleB.GetDestAndAdvance() = 1.0;
	//	}
	//}
	//else
	//{
	//	const int32 BytesPerPixel = 4;
	//	int32 IntSizeX = Texture->GetSizeX();
	//	int32 IntSizeY = Texture->GetSizeY();
	//	float SizeX = (float)Texture->GetSizeX();
	//	float SizeY = (float)Texture->GetSizeY();

	//	for (int32 i = 0; i < Context.NumInstances; ++i)
	//	{
	//		float X = fmodf(XParam.GetAndAdvance() * SizeX, SizeX);
	//		float Y = fmodf(YParam.GetAndAdvance() * SizeY, SizeY);
	//		int32 XInt = trunc(X);
	//		int32 YInt = trunc(Y);
	//		int32 SampleIdx = YInt * IntSizeX * BytesPerPixel + XInt * BytesPerPixel;
	//		ensure(CPUTextureData.Num() > SampleIdx);
	//		uint8 B0 = CPUTextureData[SampleIdx + 0];
	//		uint8 G0 = CPUTextureData[SampleIdx + 1];
	//		uint8 R0 = CPUTextureData[SampleIdx + 2];
	//		uint8 A0 = CPUTextureData[SampleIdx + 3];

	//		*OutSampleR.GetDestAndAdvance() = ((float)R0) / 255.0f;
	//		*OutSampleG.GetDestAndAdvance() = ((float)G0) / 255.0f;
	//		*OutSampleB.GetDestAndAdvance() = ((float)B0) / 255.0f;
	//		*OutSampleA.GetDestAndAdvance() = ((float)A0) / 255.0f;
	//	}
	//}

}

bool UNiagaraDataInterfaceVolume::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	FString HLSLReadTextureName = TextureName + TEXT("Read") + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLWriteCellTextureName = TextureName + TEXT("WriteCell") + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLReadSamplerName = SamplerName + TEXT("Read") + ParamInfo.DataInterfaceHLSLSymbol;

	FString HLSLTextureFramesName = TextureName + TEXT("XYFrames") + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLTextureDimsName = TextureName + TEXT("Dims2d") + ParamInfo.DataInterfaceHLSLSymbol;

	if (DefinitionFunctionName == SampleVolumeName)
	{
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_UVW, out float3 Out_Value) \n{\n");
		// PseudoVolumeTexture(Texture2D Tex, SamplerState TexSampler, float3 inPos, float2 xysize, float numframes,
		//    uint mipmode = 0, float miplevel = 0, float2 InDDX = 0, float2 InDDY = 0)
		OutHLSL += TEXT("\tfloat3 inPos = In_UVW;\n");
		OutHLSL += TEXT("\tfloat2 xySize = ") + HLSLTextureFramesName + TEXT(".xy;\n");
		OutHLSL += TEXT("\tfloat numFrames = ") + HLSLTextureFramesName + TEXT(".x * ") + HLSLTextureFramesName + TEXT(".y;\n");
		OutHLSL += TEXT("\t Out_Value = PseudoVolumeTexture(") + HLSLReadTextureName + TEXT(", ") + HLSLReadSamplerName + TEXT(", inPos, xySize, numFrames).rgb;\n");
		OutHLSL += TEXT("\n}\n");
	}
	else if (DefinitionFunctionName == WriteToVolumeName)
	{
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_UVW, in float3 In_Value, out float3 Out_Value ) \n{\n");
		OutHLSL += TEXT("\tfloat3 inPos = In_UVW;\n");
		OutHLSL += TEXT("\tfloat2 xysize = ") + HLSLTextureFramesName + TEXT(".xy;\n");
		OutHLSL += TEXT("\tfloat numframes = ") + HLSLTextureFramesName + TEXT(".x * ") + HLSLTextureFramesName + TEXT(".y;\n");
		OutHLSL += TEXT("\tfloat zframe = floor(inPos.z * numframes);\n");
		OutHLSL += TEXT("\tfloat2 uv = frac(inPos.xy) / xysize;\n");
		OutHLSL += TEXT("\tfloat2 curframe = Tile1Dto2D(xysize.x, zframe) / xysize;\n");
		OutHLSL += TEXT("\tfloat2 targetUV = saturate(uv + curframe);\n");		
		OutHLSL += TEXT("\tfloat2 halfPixelOffset = float2(0.5f, 0.5f); \n");
		OutHLSL += TEXT("\tuint2 TargetIdx = (uint2)floor(targetUV * ") + HLSLTextureDimsName + TEXT(" + halfPixelOffset);\n");
		OutHLSL += TEXT("\t") + HLSLWriteCellTextureName + TEXT("[TargetIdx] = In_Value;\n");
		OutHLSL += TEXT("\tOut_Value = In_Value;\n");
		OutHLSL += TEXT("\n}\n");
	}
	else if (DefinitionFunctionName == ReadFromClosestVolumeCellName)
	{
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_UVW, out float3 Out_Value ) \n{\n");
		OutHLSL += TEXT("\tfloat3 inPos = In_UVW;\n");
		OutHLSL += TEXT("\tfloat2 xysize = ") + HLSLTextureFramesName + TEXT(".xy;\n");
		OutHLSL += TEXT("\tfloat numframes = ") + HLSLTextureFramesName + TEXT(".x * ") + HLSLTextureFramesName + TEXT(".y;\n");
		OutHLSL += TEXT("\tfloat zframe = floor(inPos.z * numframes);\n");
		OutHLSL += TEXT("\tfloat2 uv = frac(inPos.xy) / xysize;\n");
		OutHLSL += TEXT("\tfloat2 curframe = Tile1Dto2D(xysize.x, zframe) / xysize;\n");
		OutHLSL += TEXT("\tfloat2 targetUV = saturate(uv + curframe);\n");
		OutHLSL += TEXT("\tfloat2 halfPixelOffset = float2(0.5f, 0.5f); \n");
		OutHLSL += TEXT("\tuint2 TargetIdx = (uint2)floor(targetUV * ") + HLSLTextureDimsName + TEXT(" + halfPixelOffset);\n");
		OutHLSL += TEXT("\tOut_Value = ") + HLSLReadTextureName + TEXT("[TargetIdx].xyz;\n");
		OutHLSL += TEXT("\n}\n");
	}

	return true;
}

void UNiagaraDataInterfaceVolume::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	FString HLSLReadTextureName = TextureName + TEXT("Read") + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLWriteCellTextureName = TextureName + TEXT("WriteCell") + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLReadSamplerName = SamplerName + TEXT("Read") + ParamInfo.DataInterfaceHLSLSymbol;

	FString HLSLTextureFramesName = TextureName + TEXT("XYFrames") + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLTextureDimsName = TextureName + TEXT("Dims2d") + ParamInfo.DataInterfaceHLSLSymbol;
	
	OutHLSL += TEXT("Texture2D ") + HLSLReadTextureName + TEXT(";\n");
	OutHLSL += TEXT("SamplerState ") + HLSLReadSamplerName + TEXT(";\n");
	OutHLSL += TEXT("RWTexture2D<float3> ") + HLSLWriteCellTextureName + TEXT(";\n");
	OutHLSL += TEXT("float2 ") + HLSLTextureDimsName + TEXT(";\n");
	OutHLSL += TEXT("float2 ") + HLSLTextureFramesName + TEXT(";\n");

}

struct FNiagaraDataInterfaceParametersCS_Volume : public FNiagaraDataInterfaceParametersCS
{
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) override
	{
		FString HLSLReadTextureName = UNiagaraDataInterfaceVolume::TextureName + TEXT("Read") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol;
		FString HLSLWriteCellTextureName = UNiagaraDataInterfaceVolume::TextureName + TEXT("WriteCell") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol;
		FString HLSLReadSamplerName = UNiagaraDataInterfaceVolume::SamplerName + TEXT("Read") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol;

		FString HLSLTextureFramesName = UNiagaraDataInterfaceVolume::TextureName + TEXT("XYFrames") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol;
		FString HLSLTextureDimsName = UNiagaraDataInterfaceVolume::TextureName + TEXT("Dims2d") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol;

		ReadTextureParam.Bind(ParameterMap, *HLSLReadTextureName);
		ReadSamplerParam.Bind(ParameterMap, *HLSLReadSamplerName);
		WriteTextureParam.Bind(ParameterMap, *HLSLWriteCellTextureName);
		TextureDimsParam.Bind(ParameterMap, *HLSLTextureDimsName);
		TextureFramesParam.Bind(ParameterMap, *HLSLTextureFramesName);
		
		if (!TextureDimsParam.IsBound())
		{
			UE_LOG(LogNiagara, Warning, TEXT("Binding failed for TextureDims %s. Was it optimized out?"), *HLSLTextureDimsName)
		}

		if (!TextureFramesParam.IsBound())
		{
			UE_LOG(LogNiagara, Warning, TEXT("Binding failed for TextureXYFrames %s. Was it optimized out?"), *HLSLTextureFramesName)
		}
	}

	virtual void Serialize(FArchive& Ar)override
	{
		Ar << ReadTextureParam;
		Ar << ReadSamplerParam;
		Ar << WriteTextureParam;
		Ar << TextureDimsParam;
		Ar << TextureFramesParam;
	}

	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface, void* PerInstanceData) const override
	{
		check(IsInRenderingThread());

		FNDIVolume_InstanceData* Inst = (FNDIVolume_InstanceData*)PerInstanceData;

		FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();
		UNiagaraDataInterfaceVolume* TextureDI = CastChecked<UNiagaraDataInterfaceVolume>(DataInterface);
		if (!Inst)
		{
			return;
		}
		check(!Inst->bSetInRT);
		UTexture *TextureA = Inst->VolumeTexA.Get();
		UTexture *TextureB = Inst->VolumeTexB.Get();
		if (!TextureA || !TextureB)
		{
			return;
		}
		Inst->CreateUAVs();

		FUnorderedAccessViewRHIParamRef WriteTextureUAV = Inst->GetWriteTextureUAV();
		FUnorderedAccessViewRHIParamRef ReadTextureUAV = Inst->GetReadTextureUAV();
		FTextureRHIParamRef ReadTextureSRV = Inst->GetReadTextureSRV();
		FSamplerStateRHIRef SamplerRef = Inst->GetReadTexture()->Resource->SamplerStateRHI;

		if (ReadTextureParam.IsBound())
		{
			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, ReadTextureUAV);
			SetTextureParameter(
				RHICmdList,
				ComputeShaderRHI,
				ReadTextureParam,
				ReadSamplerParam,
				SamplerRef,
				ReadTextureSRV
			);
		}

		if (TextureDimsParam.IsBound())
		{
			SetShaderValue(RHICmdList, ComputeShaderRHI, TextureDimsParam, TextureDI->Resolution2D, 0);
		}

		if (TextureFramesParam.IsBound())
		{
			SetShaderValue(RHICmdList, ComputeShaderRHI, TextureFramesParam, TextureDI->XYFrames, 0);
		}

		if (WriteTextureParam.IsBound())
		{
			RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, WriteTextureUAV);
			RHICmdList.SetUAVParameter(ComputeShaderRHI, WriteTextureParam.GetBaseIndex(), WriteTextureUAV);
		}
		Inst->bSetInRT = true;
	}

	virtual void Unset(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface, void* PerInstanceData) const override
	{
		check(IsInRenderingThread());

		FNDIVolume_InstanceData* Inst = (FNDIVolume_InstanceData*)PerInstanceData;

		FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();
		UNiagaraDataInterfaceVolume* TextureDI = CastChecked<UNiagaraDataInterfaceVolume>(DataInterface);
		if (!Inst)
		{
			return;
		}

		check(Inst->bSetInRT);

		if (ReadTextureParam.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ComputeShaderRHI,
				ReadTextureParam,
				ReadSamplerParam,
				TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI(), 
				FTextureRHIParamRef()
			);
		}
		
		if (WriteTextureParam.IsBound())
		{
			RHICmdList.SetUAVParameter(ComputeShaderRHI, WriteTextureParam.GetBaseIndex(), FUnorderedAccessViewRHIRef());
			Inst->bSwap = !Inst->bSwap;
		}

		Inst->bSetInRT = false;
	}


private:
	FShaderResourceParameter ReadTextureParam;
	FShaderResourceParameter ReadSamplerParam; 
	FShaderResourceParameter WriteTextureParam;
	FShaderParameter TextureDimsParam;
	FShaderParameter TextureFramesParam;
};


FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceVolume::ConstructComputeParameters()const
{
	return new FNiagaraDataInterfaceParametersCS_Volume();
}


#undef LOCTEXT_NAMESPACE