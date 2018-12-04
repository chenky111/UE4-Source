// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PointCloudVertexFactory.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FPointCloudVertexFactoryParameters, "PointCloudVF");

/**
 * Shader parameters for the vector field visualization vertex factory.
 */
class FPointCloudVertexFactoryShaderParameters :
	public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		ColorMask.Bind(ParameterMap, TEXT("ColorMask"));
		PointSize.Bind(ParameterMap, TEXT("PointSize"));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << ColorMask;
		Ar << PointSize;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* InVertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		FPointCloudVertexFactory* VertexFactory = (FPointCloudVertexFactory*)InVertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();

		SetUniformBufferParameter(RHICmdList, VertexShaderRHI, Shader->GetUniformBufferParameter<FPointCloudVertexFactoryParameters>(), VertexFactory->GetPointCloudVertexFactoryUniformBuffer());

		SetShaderValue(RHICmdList, VertexShaderRHI, ColorMask, VertexFactory->GetColorMask());
		SetShaderValue(RHICmdList, VertexShaderRHI, PointSize, VertexFactory->GetPointSize());
	}

	virtual uint32 GetSize() const override { return sizeof(*this); }

private:
	FShaderParameter ColorMask;
	FShaderParameter PointSize;
};

/**
 * Vertex declaration for point clouds. Verts aren't used so this is to make complaints go away
 */
class FPointCloudVertexDeclaration :
	public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4)));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
TGlobalResource<FPointCloudVertexDeclaration> GPointCloudVertexDeclaration;

/**
 * A dummy vertex buffer to bind when rendering point clouds. This prevents
 * some D3D debug warnings about zero-element input layouts but is not strictly
 * required.
 */
class FDummyVertexBuffer :
	public FVertexBuffer
{
public:

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(sizeof(FVector4) * 4, BUF_Static, CreateInfo, BufferData);
		FVector4* DummyContents = (FVector4*)BufferData;
//@todo - joeg do I need a quad's worth?
		DummyContents[0] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[1] = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
		DummyContents[2] = FVector4(0.0f, 1.0f, 0.0f, 0.0f);
		DummyContents[3] = FVector4(1.0f, 1.0f, 0.0f, 0.0f);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};
TGlobalResource<FDummyVertexBuffer> GDummyPointCloudVertexBuffer;

void FPointCloudVertexFactory::InitRHI()
{
	FVertexStream Stream;

	// No streams should currently exist.
	check( Streams.Num() == 0 );

	Stream.VertexBuffer = &GDummyPointCloudVertexBuffer;
	Stream.Stride = sizeof(FVector4);
	Stream.Offset = 0;
	Streams.Add(Stream);

	// Set the declaration.
	check(IsValidRef(GPointCloudVertexDeclaration.VertexDeclarationRHI));
	SetDeclaration(GPointCloudVertexDeclaration.VertexDeclarationRHI);
}

void FPointCloudVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

bool FPointCloudVertexFactory::ShouldCompilePermutation(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	// We exclusively use manual fetch, so we need that supported
	return RHISupportsManualVertexFetch(Platform);
}

FVertexFactoryShaderParameters* FPointCloudVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FPointCloudVertexFactoryShaderParameters() : nullptr;
}

void FPointCloudVertexFactory::SetParameters(const FPointCloudVertexFactoryParameters& InUniformParameters, const uint32 InMask, const float InSize)
{
	UniformBuffer = FPointCloudVertexFactoryBufferRef::CreateUniformBufferImmediate(InUniformParameters, UniformBuffer_MultiFrame);
	ColorMask = InMask;
	PointSize = InSize;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FPointCloudVertexFactory, "/Engine/Private/PointCloudVertexFactory.ush", true, false, false, false, false);
