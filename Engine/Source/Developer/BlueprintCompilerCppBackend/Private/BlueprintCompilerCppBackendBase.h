// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma  once

#include "CoreMinimal.h"
#include "KismetCompiledFunctionContext.h"
#include "BlueprintCompilerCppBackendInterface.h"
#include "Engine/Blueprint.h" // for FCompilerNativizationOptions

class UUserDefinedEnum;
class UUserDefinedStruct;
struct FEmitterLocalContext;

/** The class generates all native code except of function body (notice InnerFunctionImplementation in not implemented) */
class FBlueprintCompilerCppBackendBase : public IBlueprintCompilerCppBackend
{
protected:
	struct FFunctionLabelInfo
	{
		TMap<FBlueprintCompiledStatement*, int32> StateMap;
		int32 StateCounter;

		FFunctionLabelInfo()
		{
			StateCounter = 0;
		}

		int32 StatementToStateIndex(FBlueprintCompiledStatement* Statement)
		{
			int32& Index = StateMap.FindOrAdd(Statement);
			if (Index == 0)
			{
				Index = ++StateCounter;
			}

			return Index;
		}
	};

	TArray<FFunctionLabelInfo> StateMapPerFunction;
	TMap<FKismetFunctionContext*, int32> FunctionIndexMap;
	FKismetFunctionContext* UberGraphContext;
	TMap<FBlueprintCompiledStatement*, int32> UberGraphStatementToExecutionGroup;
public:

	// IBlueprintCompilerCppBackend implementation
	virtual FString GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly, const FCompilerNativizationOptions& NativizationOptions, FString& OutCppBody) override;
	virtual void GenerateCodeFromEnum(UUserDefinedEnum* SourceEnum, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode) override;
	virtual void GenerateCodeFromStruct(UUserDefinedStruct* SourceStruct, const FCompilerNativizationOptions& NativizationOptions, FString& OutHeaderCode, FString& OutCPPCode) override;
	virtual FString GenerateWrapperForClass(UClass* SourceClass, const FCompilerNativizationOptions& NativizationOptions) override;
	// end of IBlueprintCompilerCppBackend implementation

	virtual ~FBlueprintCompilerCppBackendBase() {}

protected:
	/*
		ConstructFunction generates definition, declaration and local variables. The actual implementation is generated by InnerFunctionImplementation.
		For example - this code is generated by ConstructFunction:

			void AMyActor_Child_C::BoxBox_Implementation(FBox2D NewParam,  FBox2D& NewParam1)
			{
				FBox2D CallFunc_BoxBox_NewParam1{};

				// this part is generated by InnerFunctionImplementation

				return;
			}

		Returns true is the generated function is not reducible.

	*/
	virtual bool InnerFunctionImplementation(FKismetFunctionContext& FunctionContext, FEmitterLocalContext& EmitterContext, int32 ExecutionGroup) = 0;
	
	int32 StatementToStateIndex(FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement* Statement)
	{
		int32 Index = FunctionIndexMap.FindChecked(&FunctionContext);
		return StateMapPerFunction[Index].StatementToStateIndex(Statement);
	}

private:
	/** Builds both the header declaration and body implementation of a function */
	void ConstructFunction(FKismetFunctionContext& FunctionContext, FEmitterLocalContext& EmitterContext, bool bGenerateStubOnly);

	static TArray<FString> ConstructFunctionDeclaration(FEmitterLocalContext &EmitterContext, FKismetFunctionContext &FunctionContext, TArray<UProperty*> &ArgumentList);

	static FString GenerateArgList(const FEmitterLocalContext &EmitterContext, const TArray<UProperty*> &ArgumentList, bool bOnlyParamName = false);

	static FString GenerateReturnType(const FEmitterLocalContext &EmitterContext, const UFunction* Function);

	void ConstructFunctionBody(FEmitterLocalContext& EmitterContext, FKismetFunctionContext &FunctionContext, int32 ExecutionGroup);

	void CleanBackend();

	static void EmitFileBeginning(const FString& CleanName, FEmitterLocalContext& EmitterContext, bool bIncludeGeneratedH = true, bool bIncludeCodeHelpersInHeader = false, bool bFullyIncludedDeclaration = false, UField* AdditionalFieldToIncludeInHeader = nullptr);

	static void EmitStructProperties(FEmitterLocalContext& EmitterContext, UStruct* SourceClass);

	static void DeclareDelegates(FEmitterLocalContext& EmitterContext, TIndirectArray<FKismetFunctionContext>& Functions);
};
