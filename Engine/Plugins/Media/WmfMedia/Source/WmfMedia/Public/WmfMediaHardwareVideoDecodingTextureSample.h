// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS

#include "Player/WmfMediaTextureSample.h"
#include "RHI.h"

#include "IMediaTextureSampleConverter.h"
#include "WmfMediaHardwareVideoDecodingRendering.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "d3d11.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "RenderUtils.h"

#include "Windows/COMPointer.h"

/**
 * Texture sample for hardware video decoding.
 */
class WMFMEDIA_API FWmfMediaHardwareVideoDecodingTextureSample :
	public FWmfMediaTextureSample, 
	public IMediaTextureSampleConverter
{
public:

	/** Default constructor. */
	FWmfMediaHardwareVideoDecodingTextureSample()
		: FWmfMediaTextureSample()
	{ }

public:

	/**
	 * Initialize shared texture from Wmf device
	 *
	 * @param InD3D11Device WMF device to create shared texture from
	 * @param InTime The sample time (in the player's local clock).
	 * @param InDuration The sample duration
	 * @param InDim texture dimension to create
	 * @param InFormat texture format to create
	 * @param InMediaTextureSampleFormat Media texture sample format
	 * @param InCreateFlags texture create flag
	 * @return The texture resource object that will hold the sample data.
	 */
	ID3D11Texture2D* InitializeSourceTexture(const TRefCountPtr<ID3D11Device>& InD3D11Device, FTimespan InTime, FTimespan InDuration, const FIntPoint& InDim, uint8 InFormat, EMediaTextureSampleFormat InMediaTextureSampleFormat);

	/**
	 * Get media texture sample converter if sample implements it
	 *
	 * @return texture sample converter
	 */
	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override
	{
		return this;
	}

	/**
	 * Texture sample convert using hardware video decoding.
	 */
	virtual void Convert(FTexture2DRHIRef InDstTexture) override
	{
		FWmfMediaHardwareVideoDecodingParameters::ConvertTextureFormat_RenderThread(this, InDstTexture);
	}

	/**
	 * Get source texture from WMF device
	 *
	 * @return Source texture
	 */
	TComPtr<ID3D11Texture2D> GetSourceTexture() const
	{
		return SourceTexture;
	}

	/**
	 * Get Destination Texture of render thread device
	 *
	 * @return Destination texture 
	 */
	FTexture2DRHIRef GetOrCreateDestinationTexture()
	{
		if (DestinationTexture.IsValid() && DestinationTexture->GetSizeX() == Dim.X && DestinationTexture->GetSizeY() == Dim.Y)
		{
			return DestinationTexture;
		}

		FRHIResourceCreateInfo CreateInfo;
		const uint32 CreateFlags = TexCreate_Dynamic | TexCreate_DisableSRVCreation;
		DestinationTexture = RHICreateTexture2D(
			Dim.X,
			Dim.Y,
			PF_NV12,
			1,
			1,
			CreateFlags,
			CreateInfo);

		return DestinationTexture;
	}

	/**
	 * Called the the sample is returned to the pool for cleanup purposes
	 */
	virtual void ShutdownPoolable() override;

private:

	/** Source Texture resource (from Wmf device). */
	TComPtr<ID3D11Texture2D> SourceTexture;

	/** D3D11 Device which create the texture, used to release the keyed mutex when the sampled is returned to the pool */
	TRefCountPtr<ID3D11Device> D3D11Device;

	/** Destination Texture resource (from Rendering device) */
	FTexture2DRHIRef DestinationTexture;
};

/** Implements a pool for WMF texture samples. */
class FWmfMediaHardwareVideoDecodingTextureSamplePool : public TMediaObjectPool<FWmfMediaHardwareVideoDecodingTextureSample> { };

#endif
