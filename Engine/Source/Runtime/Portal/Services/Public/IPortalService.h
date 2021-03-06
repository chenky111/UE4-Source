// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPortalService
{
public:

	/** Virtual destructor. */
	virtual ~IPortalService() { }

	virtual bool IsAvailable() const = 0;
};
