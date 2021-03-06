// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanQuery.cpp: Vulkan query RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanResources.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "EngineGlobals.h"

#if VULKAN_QUERY_CALLSTACK
#include "HAL/PlatformStackwalk.h"
#endif

TAutoConsoleVariable<int32> GSubmitOcclusionBatchCmdBufferCVar(
	TEXT("r.Vulkan.SubmitOcclusionBatchCmdBuffer"),
	1,
	TEXT("1 to submit the cmd buffer after end occlusion query batch (default)"),
	ECVF_RenderThreadSafe
);

constexpr uint32 GMinNumberOfQueriesInPool = 256;

#if PLATFORM_ANDROID
constexpr int32 NUM_FRAMES_TO_WAIT_REUSE_POOL = 5;
// numbers of frame to wait before releasing QueryPool
constexpr uint32 NUM_FRAMES_TO_WAIT_RELEASE_POOL = 10; 
#else
constexpr int32 NUM_FRAMES_TO_WAIT_REUSE_POOL = 10;
constexpr uint32 NUM_FRAMES_TO_WAIT_RELEASE_POOL = MAX_uint32; // never release
#endif

/*
FCriticalSection GOcclusionQueryCS;
const uint32 GMaxLifetimeTimestampQueries = 1024;

FVulkanRenderQuery::FVulkanRenderQuery(FVulkanDevice* Device, ERenderQueryType InQueryType)
	: QueryType(InQueryType)
{
	INC_DWORD_STAT(STAT_VulkanNumQueries);
}

FVulkanRenderQuery::~FVulkanRenderQuery()
{
	DEC_DWORD_STAT(STAT_VulkanNumQueries);
}

inline void FVulkanRenderQuery::Begin(FVulkanCmdBuffer* InCmdBuffer)
{
	BeginCmdBuffer = InCmdBuffer;
	check(State == EState::Reset);
	if (QueryType == RQT_Occlusion)
	{
		check(Pool);
		VulkanRHI::vkCmdBeginQuery(InCmdBuffer->GetHandle(), Pool->GetHandle(), QueryIndex, VK_QUERY_CONTROL_PRECISE_BIT);
		State = EState::InBegin;
		LastPoolReset = Pool->NumResets;
	}
	else
	{
		ensureMsgf(0, TEXT("Timer queries do NOT support Begin()!"));
	}
}

inline void FVulkanRenderQuery::End(FVulkanCmdBuffer* InCmdBuffer)
{
	ensure(QueryType != RQT_Occlusion || BeginCmdBuffer == InCmdBuffer);
	if (QueryType == RQT_Occlusion)
	{
		check(State == EState::InBegin);
		check(Pool);
		VulkanRHI::vkCmdEndQuery(InCmdBuffer->GetHandle(), Pool->GetHandle(), QueryIndex);
		check(LastPoolReset == Pool->NumResets);
	}
	else
	{
		check(State == EState::Reset);
		BeginCmdBuffer = InCmdBuffer;
		VulkanRHI::vkCmdWriteTimestamp(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Pool->GetHandle(), QueryIndex);
		LastPoolReset = Pool->NumResets;
	}
	State = EState::InEnd;
}

bool FVulkanRenderQuery::GetResult(FVulkanDevice* Device, uint64& OutResult, bool bWait)
{
FScopeLock ScopeLock(&GOcclusionQueryCS);
	if (State == EState::HasResults)
	{
		OutResult = Result;
		return true;
	}

	check(Pool);
	check(State == EState::InEnd);
	if (QueryType == RQT_Occlusion)
	{
		check(IsInRenderingThread());
		if (((FVulkanOcclusionQueryPool*)Pool)->GetResults(QueryIndex, bWait, Result))
		{
			check(LastPoolReset == Pool->NumResets);
			State = EState::HasResults;
			OutResult = Result;
			return true;
		}
	}
	else
	{
		check(IsInRenderingThread() || IsInRHIThread());
		if (((FVulkanTimestampQueryPool*)Pool)->GetResults(QueryIndex, bWait, Result))
		{
			check(LastPoolReset == Pool->NumResets);
			State = EState::HasResults;
			OutResult = Result;
			return true;
		}
	}
	return false;
}
*/
FVulkanQueryPool::FVulkanQueryPool(FVulkanDevice* InDevice, uint32 InMaxQueries, VkQueryType InQueryType)
	: VulkanRHI::FDeviceChild(InDevice)
	, QueryPool(VK_NULL_HANDLE)
	, ResetEvent(VK_NULL_HANDLE)
	, MaxQueries(InMaxQueries)
	, QueryType(InQueryType)
{
	INC_DWORD_STAT(STAT_VulkanNumQueryPools);
	VkQueryPoolCreateInfo PoolCreateInfo;
	ZeroVulkanStruct(PoolCreateInfo, VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
	PoolCreateInfo.queryType = QueryType;
	PoolCreateInfo.queryCount = MaxQueries;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateQueryPool(Device->GetInstanceHandle(), &PoolCreateInfo, VULKAN_CPU_ALLOCATOR, &QueryPool));

	VkEventCreateInfo EventCreateInfo;
	ZeroVulkanStruct(EventCreateInfo, VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
	VERIFYVULKANRESULT(VulkanRHI::vkCreateEvent(Device->GetInstanceHandle(), &EventCreateInfo, VULKAN_CPU_ALLOCATOR, &ResetEvent));
	
	QueryOutput.AddZeroed(MaxQueries);
}

FVulkanQueryPool::~FVulkanQueryPool()
{
	DEC_DWORD_STAT(STAT_VulkanNumQueryPools);
	VulkanRHI::vkDestroyQueryPool(Device->GetInstanceHandle(), QueryPool, VULKAN_CPU_ALLOCATOR);
	VulkanRHI::vkDestroyEvent(Device->GetInstanceHandle(), ResetEvent, VULKAN_CPU_ALLOCATOR);
	
	QueryPool = VK_NULL_HANDLE;
	ResetEvent = VK_NULL_HANDLE;
}

/*
inline VkResult FVulkanQueryPool::InternalGetQueryPoolResults(uint32 FirstQuery, uint32 NumQueries, VkQueryResultFlags ExtraFlags)
{
	VkResult Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, FirstQuery, NumQueries, NumQueries * sizeof(uint64), QueryOutput.GetData() + FirstQuery, sizeof(uint64), VK_QUERY_RESULT_64_BIT | ExtraFlags);
	return Result;
}

inline int32 FVulkanQueryPool::AllocateQuery()
{
	FScopeLock ScopeLock(&GOcclusionQueryCS);
	int32 UsedQuery = NumUsedQueries;
	++NumUsedQueries;
	checkf((uint32)UsedQuery < MaxQueries, TEXT("Internal error allocating query! UsedQuery=%d NumUsedQueries=%d MaxQueries=%d"), UsedQuery, NumUsedQueries, MaxQueries);
	return UsedQuery;
}
*/

bool FVulkanOcclusionQueryPool::CanBeReused()
{
	const uint64 NumWords = NumUsedQueries / 64;
	for (int32 Index = 0; Index < NumWords; ++Index)
	{
		if (AcquiredIndices[Index])
		{
			return false;
		}
	}

	const uint64 Remaining = NumUsedQueries % 64;
	const uint64 Mask = ((uint64)1 << (uint64)Remaining) - 1;
	return Mask == 0 || (AcquiredIndices[NumWords] & Mask) == 0;
}

bool FVulkanOcclusionQueryPool::InternalTryGetResults(bool bWait)
{
	check(CmdBuffer);
	check(State == EState::RHIT_PostEndBatch);

	VkResult Result = VK_NOT_READY;
	if (VulkanRHI::vkGetEventStatus(Device->GetInstanceHandle(), ResetEvent) == VK_EVENT_SET)
	{
		Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, 0, NumUsedQueries, NumUsedQueries * sizeof(uint64), QueryOutput.GetData(), sizeof(uint64), VK_QUERY_RESULT_64_BIT);
				if (Result == VK_SUCCESS)
		{
			State = EState::RT_PostGetResults;
			return true;
		}
	}

	if (Result == VK_NOT_READY)
	{
		if (bWait)
		{
			uint32 IdleStart = FPlatformTime::Cycles();

			SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);

			// We'll do manual wait
			double StartTime = FPlatformTime::Seconds();

			ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
			bool bSuccess = false;
			int32 NumLoops = 0;
			while (!bSuccess)
			{
				FPlatformProcess::SleepNoStats(0);

				// pump RHIThread to make sure these queries have actually been submitted to the GPU.
				if (IsInActualRenderingThread())
				{
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
				}

				if (VulkanRHI::vkGetEventStatus(Device->GetInstanceHandle(), ResetEvent) == VK_EVENT_SET)
				{
					Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, 0, NumUsedQueries, NumUsedQueries * sizeof(uint64), QueryOutput.GetData(), sizeof(uint64), VK_QUERY_RESULT_64_BIT);
				}

				if (Result == VK_SUCCESS)
				{
					bSuccess = true;
					break;
				}
				else if (Result == VK_NOT_READY)
				{
					bSuccess = false;
				}
				else
				{
					bSuccess = false;
					VERIFYVULKANRESULT(Result);
				}

				// timer queries are used for Benchmarks which can stall a bit more
				const double TimeoutValue = (QueryType == VK_QUERY_TYPE_TIMESTAMP) ? 2.0 : 0.5;
				// look for gpu stuck/crashed
				if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
				{
					if (QueryType == VK_QUERY_TYPE_OCCLUSION)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion results. (%.1f s)"), TimeoutValue);
					}
					else
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion/timer results. (%.1f s)"), TimeoutValue);
					}
					return false;
				}

				++NumLoops;
			}

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			State = EState::RT_PostGetResults;
			return true;
		}
	}
	else
	{
		VERIFYVULKANRESULT(Result);
	}
/*
	FScopeLock ScopeLock(&GOcclusionQueryCS);
	check(!bHasResults);
	VkResult Result = VK_NOT_READY;
	bool bFencePassed = false;
	if (CmdBuffer->GetFenceSignaledCounter() > FenceCounter)
	{
		bFencePassed = true;
		Result = InternalGetQueryPoolResults(0);
		if (Result == VK_SUCCESS)
		{
			bHasResults = true;
			return true;
		}
	}

	if (Result == VK_NOT_READY)
	{
		if (bWait)
		{
			uint32 IdleStart = FPlatformTime::Cycles();

			SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);

			// We'll do manual wait
			double StartTime = FPlatformTime::Seconds();

			ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
			bool bSuccess = false;
			int32 NumLoops = 0;
			while (!bSuccess)
			{
				FPlatformProcess::SleepNoStats(0);

				// pump RHIThread to make sure these queries have actually been submitted to the GPU.
				if (IsInActualRenderingThread())
				{
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
				}

				Result = InternalGetQueryPoolResults(0);
				if (Result == VK_SUCCESS)
				{
					bSuccess = true;
					break;
				}
				else if (Result == VK_NOT_READY)
				{
					bSuccess = false;
				}
				else
				{
					bSuccess = false;
					VERIFYVULKANRESULT(Result);
				}

				// timer queries are used for Benchmarks which can stall a bit more
				const double TimeoutValue = (QueryType == VK_QUERY_TYPE_TIMESTAMP) ? 2.0 : 0.5;
				// look for gpu stuck/crashed
				if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
				{
					if (QueryType == VK_QUERY_TYPE_OCCLUSION)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion results. (%.1f s)"), TimeoutValue);
					}
					else
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion/timer results. (%.1f s)"), TimeoutValue);
					}
					return false;
				}

				++NumLoops;
			}

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			bHasResults = true;
			return true;
		}
	}
	else
	{
		VERIFYVULKANRESULT(Result);
	}
	*/
	return false;
}

/*
void FVulkanQueryPool::ResetAll(FVulkanCmdBuffer* InCmdBuffer)
{
	VulkanRHI::vkCmdResetQueryPool(InCmdBuffer->GetHandle(), QueryPool, 0, MaxQueries);
	++NumResets;
}
*/

void FVulkanOcclusionQueryPool::SetFence(FVulkanCmdBuffer* InCmdBuffer)
{
	check(InCmdBuffer);
	CmdBuffer = InCmdBuffer;
	FenceCounter = InCmdBuffer->GetFenceSignaledCounter();
}

void FVulkanOcclusionQueryPool::Reset(FVulkanCmdBuffer* InCmdBuffer, uint32 InFrameNumber)
{
/*
	check(!InCmdBuffer || InCmdBuffer->GetFenceSignaledCounter() > FenceCounter);
*/
	//ensure(State == EState::Undefined || State == EState::RT_PostGetResults);
	FMemory::Memzero(AcquiredIndices.GetData(), AcquiredIndices.GetAllocatedSize());
/*
	CmdBuffer = nullptr;
	FenceCounter = UINT32_MAX;
*/
	NumUsedQueries = 0;
	FrameNumber = InFrameNumber;
/*
	bHasResults = false;
*/
	VulkanRHI::vkResetEvent(Device->GetInstanceHandle(), ResetEvent);
	VulkanRHI::vkCmdResetQueryPool(InCmdBuffer->GetHandle(), QueryPool, 0, MaxQueries);
	
	// workaround for apparent cache-flush bug in AMD driver implementation of vkCmdResetQueryPool
	if (IsRHIDeviceAMD())
	{
		VkMemoryBarrier barrier;
		ZeroVulkanStruct(barrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		VulkanRHI::vkCmdPipelineBarrier(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, 0, 0, 0);
	}
	
	VulkanRHI::vkCmdSetEvent(InCmdBuffer->GetHandle(), ResetEvent, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	//FVulkanQueryPool::ResetAll(InCmdBuffer);

	State = EState::RHIT_PostBeginBatch;
}

bool FVulkanTimingQuery::InternalGetResult(uint64& OutTime, bool bWait)
{
	ensure(State == EState::Written);
	if (Pool->TryGetResults(this, IndexInPool, bWait, Result))
	{
		State = EState::HasResult;
		OutTime = Result;
		return true;
	}

	return false;
}

VkResult FVulkanTimestampQueryPool::InternalGetQueryPoolResults(FVulkanTimingQuery* Query, uint32 QueryIndex, uint64& OutResults)
{
	uint32 PendingQueries = (QueueTail <= QueueHead) ? (QueueHead - QueueTail) : (MaxQueries - QueueTail) + QueueHead;
	uint32 FetchingQueries = (QueueTail <= QueryIndex) ? (QueryIndex - QueueTail) + 1 : (MaxQueries - QueueTail) + QueryIndex + 1;

	if (PendingQueries < FetchingQueries) //if we try to fetch more than are pending that we have the result already
	{
		OutResults = QueryOutput[QueryIndex];
		ensure(QueryAllocation[QueryIndex] == Query);
		QueryAllocation[QueryIndex] = nullptr;
		return VK_SUCCESS;
	}
	else if (QueryIndex >= QueueTail)
	{
		uint32 NumQueries = QueryIndex - QueueTail + 1;
		VkResult Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, QueueTail, NumQueries, sizeof(uint64) * NumQueries, &QueryOutput[QueueTail], sizeof(uint64), VK_QUERY_RESULT_64_BIT);
		if (Result == VK_SUCCESS)
		{
			if (!IsInRenderingThread() || !IsRunningRHIInSeparateThread())
			{
				FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetActiveCmdBuffer();
				VulkanRHI::vkCmdResetQueryPool(CmdBuffer->GetHandle(), QueryPool, QueueTail, NumQueries);
			}
			else
			{
				check(Query->Pool == this);
				FRHICommandListExecutor::GetImmediateCommandList().EnqueueLambda([NumQueries, QueueTail = QueueTail, QueryPool = Query->Pool]
				(FRHICommandListImmediate& RHICommandList)
				{
					FVulkanCmdBuffer* CmdBuffer = static_cast<FVulkanCommandListContext&>(RHICommandList.GetContext()).GetCommandBufferManager()->GetActiveCmdBuffer();
					VulkanRHI::vkCmdResetQueryPool(CmdBuffer->GetHandle(), QueryPool->QueryPool, QueueTail, NumQueries);
				});
			}
			OutResults = QueryOutput[QueryIndex];
			ensure(QueryAllocation[QueryIndex] == Query);
			QueryAllocation[QueryIndex] = nullptr;
			QueueTail = QueryIndex + 1;
		}
		return Result;
	}
	else
	{
		FVulkanTimingQuery* SavedQuery = QueryAllocation[MaxQueries - 1];
		VkResult FirstHalf = InternalGetQueryPoolResults(SavedQuery, MaxQueries - 1, OutResults);
		QueryAllocation[MaxQueries - 1] = SavedQuery;
		if (FirstHalf == VK_SUCCESS)
		{
			QueueTail = 0;
			VkResult SecondHalf = InternalGetQueryPoolResults(Query, QueryIndex, OutResults);
			if (SecondHalf == VK_SUCCESS)
			{
				QueueTail = QueryIndex + 1;
			}
			return SecondHalf;
		}
		return FirstHalf;
	}
}

bool FVulkanTimestampQueryPool::TryGetResults(FVulkanTimingQuery* Query, uint32 QueryIndex, bool bWait, uint64& OutResults)
{
	VkResult Result = InternalGetQueryPoolResults(Query, QueryIndex, OutResults);
	if (Result == VK_SUCCESS)
	{
		return true;
	}
	else if (Result == VK_NOT_READY)
	{
		if (bWait)
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);

			// We'll do manual wait
			double StartTime = FPlatformTime::Seconds();

			ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
			bool bSuccess = false;
			while (!bSuccess)
			{
				FPlatformProcess::SleepNoStats(0);

				// pump RHIThread to make sure these queries have actually been submitted to the GPU.
				if (IsInActualRenderingThread())
				{
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
				}

				Result = InternalGetQueryPoolResults(Query, QueryIndex, OutResults);
				if (Result == VK_SUCCESS)
				{
					bSuccess = true;
					break;
				}
				else if (Result == VK_NOT_READY)
				{
					bSuccess = false;
				}
				else
				{
					bSuccess = false;
					VERIFYVULKANRESULT(Result);
				}

				// timer queries are used for Benchmarks which can stall a bit more
				const double TimeoutValue = (QueryType == VK_QUERY_TYPE_TIMESTAMP) ? 2.0 : 0.5;
				// look for gpu stuck/crashed
				if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
				{
					UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on timer results. (%.1f s)"), TimeoutValue);
					return false;
				}
			}

			return true;
		}
	}
	else
	{
		VERIFYVULKANRESULT(Result);
	}

	return false;
}


void FVulkanCommandListContext::BeginOcclusionQueryBatch(FVulkanCmdBuffer* CmdBuffer, uint32 NumQueriesInBatch)
{
	ensure(IsImmediate());
/*
	FScopeLock ScopeLock(&GOcclusionQueryCS);
*/
	checkf(!CurrentOcclusionQueryPool, TEXT("BeginOcclusionQueryBatch called without corresponding EndOcclusionQueryBatch!"));
	CurrentOcclusionQueryPool = Device->AcquireOcclusionQueryPool(NumQueriesInBatch);
	ensure(CmdBuffer->IsOutsideRenderPass());
	CurrentOcclusionQueryPool->Reset(CmdBuffer, GFrameNumberRenderThread);
}


inline bool FVulkanOcclusionQueryPool::IsStalePool() const
{
	return FrameNumber + NUM_FRAMES_TO_WAIT_REUSE_POOL < GFrameNumberRenderThread;
}

void FVulkanOcclusionQueryPool::FlushAllocatedQueries()
{
	for (int32 Index = 0; Index < AcquiredIndices.Num(); ++Index)
	{
		uint32 QueryIndex = 0;
		uint64 Acquired = AcquiredIndices[Index];
		while (Acquired)
		{
			if (Acquired & 1)
			{
				FVulkanOcclusionQuery* Query =AllocatedQueries[QueryIndex + (Index * 64)];
				Query->State = Query->State == FVulkanOcclusionQuery::EState::RT_GotResults ? FVulkanOcclusionQuery::EState::FlushedFromPoolHadResults : FVulkanOcclusionQuery::EState::Undefined;
				Query->Pool = nullptr;
				Query->IndexInPool = UINT32_MAX;

				AllocatedQueries[QueryIndex + (Index * 64)] = nullptr;
			}
			Acquired = Acquired >> (uint64)1;
			QueryIndex++;
		}

		AcquiredIndices[Index] = Acquired;
	}
}

FVulkanOcclusionQueryPool* FVulkanDevice::AcquireOcclusionQueryPool(uint32 NumQueries)
{
	// At least add one query
	NumQueries = FMath::Max(1u, NumQueries);
	NumQueries = AlignArbitrary(NumQueries, GMinNumberOfQueriesInPool);

	bool bChanged = false;
	for (int32 Index = UsedOcclusionQueryPools.Num() - 1; Index >= 0; --Index)
	{
		FVulkanOcclusionQueryPool* Pool = UsedOcclusionQueryPools[Index];
		if (Pool->CanBeReused())
		{
			UsedOcclusionQueryPools.RemoveAtSwap(Index);
			FreeOcclusionQueryPools.Add(Pool);
			Pool->FreedFrameNumber = GFrameNumberRenderThread;
			bChanged = true;
		}
		else if (Pool->IsStalePool())
		{
			Pool->FlushAllocatedQueries();
			UsedOcclusionQueryPools.RemoveAtSwap(Index);
			FreeOcclusionQueryPools.Add(Pool);
			Pool->FreedFrameNumber = GFrameNumberRenderThread;
			bChanged = true;
		}
	}

	if (FreeOcclusionQueryPools.Num() > 0)
	{
		if (bChanged)
		{
			FreeOcclusionQueryPools.Sort([](FVulkanOcclusionQueryPool& A, FVulkanOcclusionQueryPool& B)
			{
				return A.GetMaxQueries() < B.GetMaxQueries();
			});
		}

		for (int32 Index = 0; Index < FreeOcclusionQueryPools.Num(); ++Index)
		{
			if (NumQueries <= FreeOcclusionQueryPools[Index]->GetMaxQueries())
			{
				FVulkanOcclusionQueryPool* Pool = FreeOcclusionQueryPools[Index];
				FreeOcclusionQueryPools.RemoveAt(Index);
				UsedOcclusionQueryPools.Add(Pool);
				return Pool;
			}
		}
	}

	FVulkanOcclusionQueryPool* Pool = new FVulkanOcclusionQueryPool(this, NumQueries);
	UsedOcclusionQueryPools.Add(Pool);
	return Pool;
}

void FVulkanDevice::ReleaseUnusedOcclusionQueryPools()
{
	if (GFrameNumberRenderThread < NUM_FRAMES_TO_WAIT_RELEASE_POOL)
	{
		return;
	}
	
	uint32 ReleaseFrame = (GFrameNumberRenderThread - NUM_FRAMES_TO_WAIT_RELEASE_POOL);

	for (int32 Index = FreeOcclusionQueryPools.Num() - 1; Index >= 0; --Index)
	{
		FVulkanOcclusionQueryPool* Pool = FreeOcclusionQueryPools[Index];
		if (ReleaseFrame > Pool->FreedFrameNumber) //-V547
		{
			delete Pool;
			FreeOcclusionQueryPools.RemoveAt(Index);
		}
	}
}

/*
FVulkanTimestampQueryPool* FVulkanDevice::PrepareTimestampQueryPool(bool& bOutRequiresReset)
{
	bOutRequiresReset = false;
	if (!TimestampQueryPool)
	{
		//#todo-rco: Create this earlier
		TimestampQueryPool = new FVulkanTimestampQueryPool(this, GMaxLifetimeTimestampQueries);
		bOutRequiresReset = true;
	}

	// Wrap around the queries
	if (TimestampQueryPool->GetNumAllocatedQueries() >= TimestampQueryPool->GetMaxQueries())
	{
		//#todo-rco: Check overlap and grow if necessary
		TimestampQueryPool->NumUsedQueries = 0;
	}

	return TimestampQueryPool;
}
*/
void FVulkanCommandListContext::EndOcclusionQueryBatch(FVulkanCmdBuffer* CmdBuffer)
{
	ensure(IsImmediate());
	check(CmdBuffer);
	checkf(CurrentOcclusionQueryPool, TEXT("EndOcclusionQueryBatch called without corresponding BeginOcclusionQueryBatch!"));
	CurrentOcclusionQueryPool->EndBatch(CmdBuffer);
	CurrentOcclusionQueryPool = nullptr;
	TransitionAndLayoutManager.EndRealRenderPass(CmdBuffer);
/*
	FScopeLock ScopeLock(&GOcclusionQueryCS);
	CurrentOcclusionQueryPool = nullptr;
	TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);
*/
	// Sync point
	if (GSubmitOcclusionBatchCmdBufferCVar.GetValueOnAnyThread())
	{
		RequestSubmitCurrentCommands();
		SafePointSubmit();
	}
}


FVulkanOcclusionQuery::FVulkanOcclusionQuery()
	: FVulkanRenderQuery(RQT_Occlusion)
{
	INC_DWORD_STAT(STAT_VulkanNumQueries);
}

FVulkanOcclusionQuery::~FVulkanOcclusionQuery()
{
	if (State != EState::Undefined)
	{
		if (IndexInPool != UINT32_MAX)
		{
			ReleaseFromPool();
		}
		else
		{
			ensure(State == EState::RT_GotResults);
		}
	}

	DEC_DWORD_STAT(STAT_VulkanNumQueries);
}

void FVulkanOcclusionQuery::ReleaseFromPool()
{
	Pool->ReleaseIndex(IndexInPool);
	State = EState::Undefined;
	IndexInPool = UINT32_MAX;
}

void FVulkanCommandListContext::ReadAndCalculateGPUFrameTime()
{
	check(IsImmediate());

	if (FVulkanPlatform::SupportsTimestampRenderQueries() && FrameTiming)
	{
		const uint64 Delta = FrameTiming->GetTiming(false);
		const double SecondsPerCycle = FPlatformTime::GetSecondsPerCycle();
		const double Frequency = double(FVulkanGPUTiming::GetTimingFrequency());
		GGPUFrameTime = FMath::TruncToInt(double(Delta) / Frequency / SecondsPerCycle);
	}
	else
	{
		GGPUFrameTime = 0;
	}
}


FRenderQueryRHIRef FVulkanDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	if (QueryType == RQT_Occlusion)
	{
		FVulkanRenderQuery* Query = new FVulkanOcclusionQuery();
		return Query;
	}
	else if (QueryType == RQT_AbsoluteTime)
	{
/*
		FVulkanTimingQuery* Query = new FVulkanTimingQuery();
		return Query;
*/
	}
	else
	{
		// Dummy!
		ensureMsgf(0, TEXT("Unknown QueryType %d"), QueryType);
	}
	FVulkanRenderQuery* Query = new FVulkanRenderQuery(QueryType);
	return Query;
}

bool FVulkanDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI, uint64& OutNumPixels, bool bWait)
{
	check(IsInRenderingThread());
	FVulkanRenderQuery* BaseQuery = ResourceCast(QueryRHI);
	if (BaseQuery->QueryType == RQT_Occlusion)
	{
		FVulkanOcclusionQuery* Query = static_cast<FVulkanOcclusionQuery*>(BaseQuery);
		if (Query->State == FVulkanOcclusionQuery::EState::RT_GotResults || Query->State == FVulkanOcclusionQuery::EState::FlushedFromPoolHadResults)
		{
			OutNumPixels = Query->Result;
			return true;
		}

		if (Query->State == FVulkanOcclusionQuery::EState::Undefined)
		{
			UE_LOG(LogVulkanRHI, Verbose, TEXT("Stale query asking for result!"));
			return false;
		}

		ensure(Query->State == FVulkanOcclusionQuery::EState::RHI_PostEnd);
		if (Query->Pool->TryGetResults(bWait))
		{
			Query->Result = Query->Pool->GetResultValue(Query->IndexInPool);
			Query->ReleaseFromPool();
			Query->State = FVulkanOcclusionQuery::EState::RT_GotResults;
			OutNumPixels = Query->Result;
			return true;
		}
	}
	else if (BaseQuery->QueryType == RQT_AbsoluteTime)
	{
/*
		FVulkanTimingQuery* Query = static_cast<FVulkanTimingQuery*>(BaseQuery);
		uint64 TimingResult;
		bool QueryResult = Query->GetResult(TimingResult, false);
		OutNumPixels = TimingResult / 1000; //RHIGetRenderQueryResult assumes microseconds where vk provided ns
		return QueryResult;
*/
	}
	/*
	FScopeLock ScopeLock(&GOcclusionQueryCS);
	check(IsInRenderingThread());

	return Query->GetResult(Device, OutResult, bWait);
	*/
	return false;
}

void FVulkanCommandListContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FVulkanRenderQuery* BaseQuery = ResourceCast(QueryRHI);
	if (BaseQuery->QueryType == RQT_Occlusion)
	{
		//#todo-rco: Temp until we get the merge straightened out
		if (!CurrentOcclusionQueryPool)
		{
			return;
		}
		check(CurrentOcclusionQueryPool);
		FVulkanOcclusionQuery* Query = static_cast<FVulkanOcclusionQuery*>(BaseQuery);
		if (Query->State == FVulkanOcclusionQuery::EState::RHI_PostEnd)
		{
			Query->ReleaseFromPool();
		}
		else if (Query->State == FVulkanOcclusionQuery::EState::RT_GotResults)
		{
			// Nothing to do here...
		}
		else
		{
			ensure(Query->State == FVulkanOcclusionQuery::EState::Undefined || Query->State == FVulkanOcclusionQuery::EState::FlushedFromPoolHadResults);
		}
		Query->State = FVulkanOcclusionQuery::EState::RHI_PostBegin;
		const uint32 IndexInPool = CurrentOcclusionQueryPool->AcquireIndex(Query);
		Query->Pool = CurrentOcclusionQueryPool;
		Query->IndexInPool = IndexInPool;
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		VulkanRHI::vkCmdBeginQuery(CmdBuffer->GetHandle(), CurrentOcclusionQueryPool->GetHandle(), IndexInPool, VK_QUERY_CONTROL_PRECISE_BIT);
	}
	else if (BaseQuery->QueryType == RQT_AbsoluteTime)
	{
		ensureMsgf(0, TEXT("Timing queries should NOT call RHIBeginRenderQuery()!"));
	}
}

void FVulkanCommandListContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FVulkanRenderQuery* BaseQuery = ResourceCast(QueryRHI);
	if (BaseQuery->QueryType == RQT_Occlusion)
	{
		//#todo-rco: Temp until we get the merge straightened out
		if (!CurrentOcclusionQueryPool)
		{
			return;
		}

		FVulkanOcclusionQuery* Query = static_cast<FVulkanOcclusionQuery*>(BaseQuery);
		ensure(Query->State == FVulkanOcclusionQuery::EState::RHI_PostBegin);
		Query->State = FVulkanOcclusionQuery::EState::RHI_PostEnd;
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		VulkanRHI::vkCmdEndQuery(CmdBuffer->GetHandle(), CurrentOcclusionQueryPool->GetHandle(), Query->IndexInPool);
	}
	else if (BaseQuery->QueryType == RQT_AbsoluteTime)
	{
/*
		FVulkanTimingQuery* Query = static_cast<FVulkanTimingQuery*>(BaseQuery);
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		Query->Pool = Query->IsPooledAtHighLevel ? Query->Pool : CmdBuffer->PrepareTimestampQueryPool();
		Query->IndexInPool = Query->Pool->AcquireIndex(Query);
		Query->State = FVulkanTimingQuery::Written;

		VulkanRHI::vkCmdWriteTimestamp(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Query->Pool->GetHandle(), Query->IndexInPool);
#if VULKAN_QUERY_CALLSTACK
		FPlatformStackWalk::CaptureStackBackTrace(Query->StackFrames, 16);
#endif
*/
	}
}

/*
FRenderQueryRHIRef FVulkanTimestampQueryPool::AllocateQuery()
{
	FVulkanTimingQuery* Query = new FVulkanTimingQuery();
	Query->State = FVulkanTimingQuery::Unused;
	Query->IsPooledAtHighLevel = true;
	Query->Pool = this;
	return Query;
}

void FVulkanTimestampQueryPool::ReleaseQuery(TRefCountPtr<FRHIRenderQuery> &Query)
{
	Query.SafeRelease();
}
*/

void FVulkanCommandListContext::WriteBeginTimestamp(FVulkanCmdBuffer* CmdBuffer)
{
	FrameTiming->StartTiming(CmdBuffer);
}

void FVulkanCommandListContext::WriteEndTimestamp(FVulkanCmdBuffer* CmdBuffer)
{
	FrameTiming->EndTiming(CmdBuffer);
}
