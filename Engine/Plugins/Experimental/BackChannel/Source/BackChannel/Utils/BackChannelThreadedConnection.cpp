// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BackChannel/Utils/BackChannelThreadedConnection.h"
#include "BackChannel/Private/BackChannelCommon.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/ScopeLock.h"


FBackChannelThreadedListener::FBackChannelThreadedListener()
{
}

FBackChannelThreadedListener::~FBackChannelThreadedListener()
{
	Stop();
}

bool FBackChannelThreadedListener::IsRunning() const
{
	return bIsRunning;
}

void FBackChannelThreadedListener::Start(TSharedRef<IBackChannelConnection> InConnection, FBackChannelListenerDelegate InDelegate)
{
	Connection = InConnection;
	Delegate = InDelegate;

	bIsRunning = true;
	bExitRequested = false;

	FRunnableThread::Create(this, TEXT("FBackChannelSocketThread"), 32 * 1024);
}

uint32 FBackChannelThreadedListener::Run()
{
	while (bExitRequested == false)
	{
		FScopeLock RunningLock(&RunningCS);

		Connection->WaitForConnection(1, [this](TSharedRef<IBackChannelConnection> NewConnection) {
			return Delegate.Execute(NewConnection);
		});
	}

	bIsRunning = false;
	return 0;
}

void FBackChannelThreadedListener::Stop()
{
	bExitRequested = true;

	if (IsRunning())
	{
		FScopeLock RunLock(&RunningCS);
	}

	bExitRequested = false;
}
