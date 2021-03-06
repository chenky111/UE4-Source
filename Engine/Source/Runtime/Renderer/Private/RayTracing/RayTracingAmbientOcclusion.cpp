// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeferredShadingRenderer.h"

#if RHI_RAYTRACING

#include "ClearQuad.h"
#include "SceneRendering.h"
#include "SceneRenderTargets.h"
#include "SceneUtils.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
#include "UniformBuffer.h"
#include "RHI/Public/PipelineStateCache.h"
#include "Raytracing/RaytracingOptions.h"

#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"

static int32 GRayTracingAmbientOcclusion = 1;
static FAutoConsoleVariableRef CVarRayTracingAmbientOcclusion(
	TEXT("r.RayTracing.AmbientOcclusion"),
	GRayTracingAmbientOcclusion,
	TEXT("Enables ray tracing ambient occlusion (default = 1)")
);

bool ShouldRenderRayTracingAmbientOcclusion()
{
	return IsRayTracingEnabled() && GRayTracingAmbientOcclusion != 0;
}

static int32 GRayTracingAmbientOcclusionSamplesPerPixel = 1;
static FAutoConsoleVariableRef CVarRayTracingAmbientOcclusionSamplesPerPixel(
	TEXT("r.RayTracing.AmbientOcclusion.SamplesPerPixel"),
	GRayTracingAmbientOcclusionSamplesPerPixel,
	TEXT("Sets the samples-per-pixel for ambient occlusion (default = 1)")
);

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FAmbientOcclusionData, )
SHADER_PARAMETER(int, SamplesPerPixel)
SHADER_PARAMETER(float, MaxRayDistance)
SHADER_PARAMETER(float, Intensity)
SHADER_PARAMETER(float, MaxNormalBias)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FAmbientOcclusionData, "AmbientOcclusion");

DECLARE_GPU_STAT_NAMED(RayTracingAmbientOcclusion, TEXT("Ray Tracing Ambient Occlusion"));

class FAmbientOcclusionRGS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FAmbientOcclusionRGS, Global)

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	FAmbientOcclusionRGS() {}
	virtual ~FAmbientOcclusionRGS() {}

	FAmbientOcclusionRGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ViewParameter.Bind(Initializer.ParameterMap, TEXT("View"));
		TLASParameter.Bind(Initializer.ParameterMap, TEXT("TLAS"));
		SceneTexturesParameter.Bind(Initializer.ParameterMap, TEXT("SceneTexturesStruct"));
		AmbientOcclusionParameter.Bind(Initializer.ParameterMap, TEXT("AmbientOcclusion"));

		OcclusionMaskUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWOcclusionMaskUAV"));
		RayDistanceUAVParameter.Bind(Initializer.ParameterMap, TEXT("RWHitDistanceUAV"));
	}

	bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ViewParameter;
		Ar << TLASParameter;
		Ar << SceneTexturesParameter;
		Ar << AmbientOcclusionParameter;
		Ar << OcclusionMaskUAVParameter;
		Ar << RayDistanceUAVParameter;
		return bShaderHasOutdatedParameters;
	}

	void Dispatch(
		FRHICommandListImmediate& RHICmdList,
		const FRayTracingScene& RayTracingScene,
		FUniformBufferRHIParamRef ViewUniformBuffer,
		FUniformBufferRHIParamRef SceneTexturesUniformBuffer,
		FUniformBufferRHIParamRef AmbientOcclusionUniformBuffer,
		FUnorderedAccessViewRHIParamRef OcclusionMaskUAV,
		FUnorderedAccessViewRHIParamRef HitDistanceUAV,
		uint32 Width, uint32 Height
	)
	{
		FRayTracingPipelineStateInitializer Initializer;

		FRayTracingShaderRHIParamRef RayGenShaderTable[] = { GetRayTracingShader() };
		Initializer.SetRayGenShaderTable(RayGenShaderTable);

		FRHIRayTracingPipelineState* Pipeline = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(Initializer); // #dxr_todo: this should be done once at load-time and cached

		FRayTracingShaderBindingsWriter GlobalResources;
		GlobalResources.Set(TLASParameter, RayTracingScene.RayTracingSceneRHI->GetShaderResourceView());
		GlobalResources.Set(ViewParameter, ViewUniformBuffer);
		GlobalResources.Set(SceneTexturesParameter, SceneTexturesUniformBuffer);
		GlobalResources.Set(AmbientOcclusionParameter, AmbientOcclusionUniformBuffer);
		GlobalResources.Set(OcclusionMaskUAVParameter, OcclusionMaskUAV);
		GlobalResources.Set(RayDistanceUAVParameter, HitDistanceUAV);

		const uint32 RayGenShaderIndex = 0;
		RHICmdList.RayTraceDispatch(Pipeline, RayGenShaderIndex, GlobalResources, Width, Height);
	}

private:
	// Input
	FShaderResourceParameter TLASParameter;
	FShaderUniformBufferParameter ViewParameter;
	FShaderUniformBufferParameter SceneTexturesParameter;
	FShaderUniformBufferParameter AmbientOcclusionParameter;

	// Output
	FShaderResourceParameter OcclusionMaskUAVParameter;
	FShaderResourceParameter RayDistanceUAVParameter;
};

IMPLEMENT_SHADER_TYPE(, FAmbientOcclusionRGS, TEXT("/Engine/Private/RayTracing/RayTracingAmbientOcclusionRGS.usf"), TEXT("AmbientOcclusionRGS"), SF_RayGen)

void FDeferredShadingSceneRenderer::RenderRayTracingAmbientOcclusion(
	FRHICommandListImmediate& RHICmdList,
	const FLightSceneInfo* SkyLightSceneInfo,
	TRefCountPtr<IPooledRenderTarget>& AmbientOcclusionMask,
	TRefCountPtr<IPooledRenderTarget>& HitDistance
)
{
	SCOPED_DRAW_EVENT(RHICmdList, RayTracingAmbientOcclusion);
	SCOPED_GPU_STAT(RHICmdList, RayTracingAmbientOcclusion);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
	Desc.Format = PF_R16F;
	Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, AmbientOcclusionMask, TEXT("RayTracingAmbientOcclusion"));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, HitDistance, TEXT("RayTracingAmbientOcclusionHitDistance"));

	// Add ambient occlusion parameters to uniform buffer
	FAmbientOcclusionData AmbientOcclusionData;
	AmbientOcclusionData.SamplesPerPixel = GRayTracingAmbientOcclusionSamplesPerPixel;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		FIntPoint ViewSize = View.ViewRect.Size();

		TShaderMapRef<FAmbientOcclusionRGS> AmbientOcclusionRayGenerationShader(GetGlobalShaderMap(FeatureLevel));
		FSceneTexturesUniformParameters SceneTextures;
		SetupSceneTextureUniformParameters(SceneContext, FeatureLevel, ESceneTextureSetupMode::All, SceneTextures);
		FUniformBufferRHIRef SceneTexturesUniformBuffer = RHICreateUniformBuffer(&SceneTextures, FSceneTexturesUniformParameters::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

		AmbientOcclusionData.MaxRayDistance = View.FinalPostProcessSettings.AmbientOcclusionRadius;
		AmbientOcclusionData.Intensity = View.FinalPostProcessSettings.AmbientOcclusionIntensity;
		AmbientOcclusionData.MaxNormalBias = GetRaytracingOcclusionMaxNormalBias();
		FUniformBufferRHIRef AmbientOcclusionUniformBuffer = RHICreateUniformBuffer(&AmbientOcclusionData, FAmbientOcclusionData::StaticStructMetadata.GetLayout(), EUniformBufferUsage::UniformBuffer_SingleDraw);

		AmbientOcclusionRayGenerationShader->Dispatch(
			RHICmdList,
			View.PerViewRayTracingScene,
			View.ViewUniformBuffer,
			SceneTexturesUniformBuffer,
			AmbientOcclusionUniformBuffer,
			AmbientOcclusionMask->GetRenderTargetItem().UAV,
			HitDistance->GetRenderTargetItem().UAV,
			ViewSize.X, ViewSize.Y
		);
	}

	FUnorderedAccessViewRHIParamRef UAVs[]
	{
		AmbientOcclusionMask->GetRenderTargetItem().UAV,
		HitDistance->GetRenderTargetItem().UAV
	};
	FComputeFenceRHIRef Fence = RHICmdList.CreateComputeFence(TEXT("RayTracingAmbientOcclusion"));
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, UAVs, ARRAY_COUNT(UAVs), Fence);

	GVisualizeTexture.SetCheckPoint(RHICmdList, AmbientOcclusionMask);
	GVisualizeTexture.SetCheckPoint(RHICmdList, HitDistance);
	SceneContext.bScreenSpaceAOIsValid = true;
}

#endif // RHI_RAYTRACING
