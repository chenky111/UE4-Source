// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IDeviceProfileSelectorModule.h"
#include "PIEPreviewDeviceEnumeration.h"
#include "RHIDefinitions.h"
#include "Misc/CommandLine.h"
#include "Widgets/SWindow.h"
#include "IPIEPreviewDeviceModule.h"
/**
* Implements the Preview Device Profile Selector module.
*/

class FPIEPreviewDeviceModule
	: public IPIEPreviewDeviceModule
{
public:
	FPIEPreviewDeviceModule() : bInitialized(false)
	{
	}

	//~ Begin IDeviceProfileSelectorModule Interface
	virtual const FString GetRuntimeDeviceProfileName() override;

	//~ End IDeviceProfileSelectorModule Interface

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	/**
	* Virtual destructor.
	*/
	virtual ~FPIEPreviewDeviceModule()
	{
	}

	virtual void ApplyPreviewDeviceState() override;
	
	virtual TSharedRef<SWindow> CreatePIEPreviewDeviceWindow(FVector2D ClientSize, FText WindowTitle, EAutoCenter AutoCenterType, FVector2D ScreenPosition, TOptional<float> MaxWindowWidth, TOptional<float> MaxWindowHeight) override;

	/** call this after the window is created and registered to the application to setup display related parameters */
	virtual void PrepareDeviceDisplay() override;
	
	virtual const FPIEPreviewDeviceContainer& GetPreviewDeviceContainer() ;
	TSharedPtr<FPIEPreviewDeviceContainerCategory> GetPreviewDeviceRootCategory() const { return EnumeratedDevices.GetRootCategory(); }

	static bool IsRequestingPreviewDevice()
	{
		FString PreviewDeviceDummy;
		return FParse::Value(FCommandLine::Get(), GetPreviewDeviceCommandSwitch(), PreviewDeviceDummy);
	}

private:
	static const TCHAR* GetPreviewDeviceCommandSwitch()
	{
		return TEXT("MobileTargetDevice=");
	}

	void InitPreviewDevice();
	static FString GetDeviceSpecificationContentDir();
	bool ReadDeviceSpecification();
	FString FindDeviceSpecificationFilePath(const FString& SearchDevice);

	void UpdateDisplayResolution();

private:
	bool bInitialized;
	FString DeviceProfile;
	FString PreviewDevice;

	FPIEPreviewDeviceContainer EnumeratedDevices;

	TSharedPtr<class FPIEPreviewDevice> Device;

	TWeakPtr<class SPIEPreviewWindow> WindowWPtr;
};
