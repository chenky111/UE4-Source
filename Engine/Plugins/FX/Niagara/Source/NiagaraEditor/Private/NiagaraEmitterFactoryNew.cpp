// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScriptFactoryNew.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEditorSettings.h"
#include "AssetData.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraConstants.h"
#include "NiagaraNodeAssignment.h"
#include "SNewEmitterDialog.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"

#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterFactory"

UNiagaraEmitterFactoryNew::UNiagaraEmitterFactoryNew(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SupportedClass = UNiagaraEmitter::StaticClass();
	bCreateNew = false;
	bEditAfterNew = true;
	bCreateNew = true;
	EmitterToCopy = nullptr;
}

bool UNiagaraEmitterFactoryNew::ConfigureProperties()
{
	TSharedRef<SNewEmitterDialog> NewEmitterDialog = SNew(SNewEmitterDialog);
	GEditor->EditorAddModalWindow(NewEmitterDialog);
	if (NewEmitterDialog->GetUserConfirmedSelection() == false)
	{
		// User cancelled or closed the dialog so abort asset creation.
		return false;
	}

	TOptional<FAssetData> SelectedEmitterAsset = NewEmitterDialog->GetSelectedEmitterAsset();
	if (SelectedEmitterAsset.IsSet())
	{
		EmitterToCopy = Cast<UNiagaraEmitter>(SelectedEmitterAsset->GetAsset());
		if (EmitterToCopy == nullptr)
		{
			FText Title = LOCTEXT("FailedToLoadTitle", "Create Default?");
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::OkCancel, EAppReturnType::Cancel,
				LOCTEXT("FailedToLoadMessage", "The selected emitter failed to load\nWould you like to create a default emitter?"),
				&Title);
			if (DialogResult == EAppReturnType::Cancel)
			{
				return false;
			}
			else
			{
				// The selected emitter couldn't be loaded but the user still wants to create a default emitter so leave
				// the emitter to copy unset, and not null to force creation of a default emitter.
				EmitterToCopy.Reset();
			}
		}
	}
	else
	{
		// User selected an empty emitter so set the emitter to copy to null.
		EmitterToCopy = nullptr;
	}

	return true;
}

UObject* UNiagaraEmitterFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraEmitter::StaticClass()));

	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	check(Settings);

	UNiagaraEmitter* NewEmitter;

	if (EmitterToCopy.IsSet())
	{
		if (EmitterToCopy.GetValue() != nullptr)
		{
			NewEmitter = Cast<UNiagaraEmitter>(StaticDuplicateObject(EmitterToCopy.GetValue(), InParent, Name, Flags, Class));
			NewEmitter->bIsPrototypeAsset = false;
			NewEmitter->PrototypeAssetDescription = FText();
		}
		else
		{
			// Create an empty emitter, source, and graph.
			NewEmitter = NewObject<UNiagaraEmitter>(InParent, Class, Name, Flags | RF_Transactional);
			NewEmitter->SimTarget = ENiagaraSimTarget::CPUSim;

			UNiagaraScriptSource* Source = NewObject<UNiagaraScriptSource>(NewEmitter, NAME_None, RF_Transactional);
			UNiagaraGraph* CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;

			// Fix up source pointers.
			NewEmitter->GraphSource = Source;
			NewEmitter->SpawnScriptProps.Script->SetSource(Source);
			NewEmitter->UpdateScriptProps.Script->SetSource(Source);
			NewEmitter->EmitterSpawnScriptProps.Script->SetSource(Source);
			NewEmitter->EmitterUpdateScriptProps.Script->SetSource(Source);
			NewEmitter->GetGPUComputeScript()->SetSource(Source);

			// Initialize the scripts for output.
			FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::EmitterSpawnScript, NewEmitter->EmitterSpawnScriptProps.Script->GetUsageId());
			FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::EmitterUpdateScript, NewEmitter->EmitterUpdateScriptProps.Script->GetUsageId());
			FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::ParticleSpawnScript, NewEmitter->SpawnScriptProps.Script->GetUsageId());
			FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::ParticleUpdateScript, NewEmitter->UpdateScriptProps.Script->GetUsageId());
		}
	}
	else if (UNiagaraEmitter* Default = Cast<UNiagaraEmitter>(Settings->DefaultEmitter.TryLoad()))
	{
		NewEmitter = Cast<UNiagaraEmitter>(StaticDuplicateObject(Default, InParent, Name, Flags, Class));
	}
	else
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("Default Emitter \"%s\" could not be loaded. Creating graph procedurally."), *Settings->DefaultEmitter.ToString());

		NewEmitter = NewObject<UNiagaraEmitter>(InParent, Class, Name, Flags | RF_Transactional);

		NewEmitter->AddRenderer(NewObject<UNiagaraSpriteRendererProperties>(NewEmitter, "Renderer"));

		UNiagaraScriptSource* Source = NewObject<UNiagaraScriptSource>(NewEmitter, NAME_None, RF_Transactional);
		if (Source)
		{
			UNiagaraGraph* CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;

			// Set pointer in script to source
			NewEmitter->GraphSource = Source;
			NewEmitter->SpawnScriptProps.Script->SetSource(Source);
			NewEmitter->UpdateScriptProps.Script->SetSource(Source);
			NewEmitter->EmitterSpawnScriptProps.Script->SetSource(Source);
			NewEmitter->EmitterUpdateScriptProps.Script->SetSource(Source);
			NewEmitter->GetGPUComputeScript()->SetSource(Source);
			NewEmitter->SimTarget = ENiagaraSimTarget::CPUSim;

			bool bCreateDefaultNodes = true;
			if (bCreateDefaultNodes)
			{
				if (Source)
				{
					UNiagaraNodeOutput* EmitterSpawnOutputNode = FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::EmitterSpawnScript, NewEmitter->EmitterSpawnScriptProps.Script->GetUsageId());
					UNiagaraNodeOutput* EmitterUpdateOutputNode = FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::EmitterUpdateScript, NewEmitter->EmitterUpdateScriptProps.Script->GetUsageId());
					UNiagaraNodeOutput* ParticleSpawnOutputNode = FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::ParticleSpawnScript, NewEmitter->SpawnScriptProps.Script->GetUsageId());
					UNiagaraNodeOutput* ParticleUpdateOutputNode = FNiagaraStackGraphUtilities::ResetGraphForOutput(*Source->NodeGraph, ENiagaraScriptUsage::ParticleUpdateScript, NewEmitter->UpdateScriptProps.Script->GetUsageId());

					{
						FStringAssetReference EmitterUpdateScriptRef(TEXT("/Niagara/Modules/Emitter/EmitterLifeCycle.EmitterLifeCycle"));
						UNiagaraScript* EmitterUpdateScript = Cast<UNiagaraScript>(EmitterUpdateScriptRef.TryLoad());
						FAssetData EmitterUpdateModuleScriptAsset(EmitterUpdateScript);
						if (EmitterUpdateOutputNode && EmitterUpdateModuleScriptAsset.IsValid())
						{
							FNiagaraStackGraphUtilities::AddScriptModuleToStack(EmitterUpdateModuleScriptAsset, *EmitterUpdateOutputNode);
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Missing %s"), *EmitterUpdateScriptRef.ToString());
						}
					}

					{
						FStringAssetReference EmitterUpdateScriptRef(TEXT("/Niagara/Modules/Emitter/SpawnRate.SpawnRate"));
						UNiagaraScript* EmitterUpdateScript = Cast<UNiagaraScript>(EmitterUpdateScriptRef.TryLoad());
						FAssetData EmitterUpdateModuleScriptAsset(EmitterUpdateScript);
						if (EmitterUpdateOutputNode && EmitterUpdateModuleScriptAsset.IsValid())
						{
							FNiagaraStackGraphUtilities::AddScriptModuleToStack(EmitterUpdateModuleScriptAsset, *EmitterUpdateOutputNode);
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Missing %s"), *EmitterUpdateScriptRef.ToString());
						}
					}

					{
						FStringAssetReference SpawnScriptRef(TEXT("/Niagara/Modules/Spawn/Location/SystemLocation.SystemLocation"));
						UNiagaraScript* SpawnScript = Cast<UNiagaraScript>(SpawnScriptRef.TryLoad());
						FAssetData SpawnModuleScriptAsset(SpawnScript);
						if (ParticleSpawnOutputNode && SpawnModuleScriptAsset.IsValid())
						{
							FNiagaraStackGraphUtilities::AddScriptModuleToStack(SpawnModuleScriptAsset, *ParticleSpawnOutputNode);
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Missing %s"), *SpawnScriptRef.ToString());
						}
					}

					{
						FStringAssetReference SpawnScriptRef(TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity"));
						UNiagaraScript* SpawnScript = Cast<UNiagaraScript>(SpawnScriptRef.TryLoad());
						FAssetData SpawnModuleScriptAsset(SpawnScript);
						if (ParticleSpawnOutputNode && SpawnModuleScriptAsset.IsValid())
						{
							UNiagaraNodeFunctionCall* CallNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(SpawnModuleScriptAsset, *ParticleSpawnOutputNode);
							if (CallNode)
							{
								FNiagaraVariable VelocityVar(FNiagaraTypeDefinition::GetVec3Def(), *(TEXT("Constants.Emitter.") + CallNode->GetFunctionName() + TEXT(".Velocity")));
								VelocityVar.SetValue(FVector(0.0f, 0.0f, 100.0f));
								bool bAddParameterIfMissing = true;
								NewEmitter->SpawnScriptProps.Script->RapidIterationParameters.SetParameterData(VelocityVar.GetData(), VelocityVar, bAddParameterIfMissing);
							}
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Missing %s"), *SpawnScriptRef.ToString());
						}

					}

					{
						if (ParticleSpawnOutputNode)
						{
							TArray<FNiagaraVariable> Vars;
							TArray<FString> Defaults;
							{
								FNiagaraVariable Var = SYS_PARAM_PARTICLES_SPRITE_SIZE;
								FString DefaultValue = FNiagaraConstants::GetAttributeDefaultValue(Var);
								Vars.Add(Var);
								Defaults.Add(DefaultValue);
							}

							{
								FNiagaraVariable Var = SYS_PARAM_PARTICLES_SPRITE_ROTATION;
								FString DefaultValue = FNiagaraConstants::GetAttributeDefaultValue(Var);
								Vars.Add(Var);
								Defaults.Add(DefaultValue);
							}

							{
								FNiagaraVariable Var = SYS_PARAM_PARTICLES_LIFETIME;
								FString DefaultValue = FNiagaraConstants::GetAttributeDefaultValue(Var);
								Vars.Add(Var);
								Defaults.Add(DefaultValue);
							}
							FNiagaraStackGraphUtilities::AddParameterModuleToStack(Vars, *ParticleSpawnOutputNode, INDEX_NONE, Defaults);

						}
					}
					
					{
						FStringAssetReference UpdateScriptRef(TEXT("/Niagara/Modules/Update/Lifetime/UpdateAge.UpdateAge"));
						UNiagaraScript* UpdateScript = Cast<UNiagaraScript>(UpdateScriptRef.TryLoad());
						FAssetData UpdateModuleScriptAsset(UpdateScript);
						if (ParticleUpdateOutputNode && UpdateModuleScriptAsset.IsValid())
						{
							FNiagaraStackGraphUtilities::AddScriptModuleToStack(UpdateModuleScriptAsset, *ParticleUpdateOutputNode);
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Missing %s"), *UpdateScriptRef.ToString());
						}
					}

					{
						FStringAssetReference UpdateScriptRef(TEXT("/Niagara/Modules/Update/Color/Color.Color"));
						UNiagaraScript* UpdateScript = Cast<UNiagaraScript>(UpdateScriptRef.TryLoad());
						FAssetData UpdateModuleScriptAsset(UpdateScript);
						if (ParticleUpdateOutputNode && UpdateModuleScriptAsset.IsValid())
						{
							FNiagaraStackGraphUtilities::AddScriptModuleToStack(UpdateModuleScriptAsset, *ParticleUpdateOutputNode);
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Missing %s"), *UpdateScriptRef.ToString());
						}
					}

					{
						FStringAssetReference UpdateScriptRef(TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"));
						UNiagaraScript* UpdateScript = Cast<UNiagaraScript>(UpdateScriptRef.TryLoad());
						FAssetData UpdateModuleScriptAsset(UpdateScript);
						if (ParticleUpdateOutputNode && UpdateModuleScriptAsset.IsValid())
						{
							FNiagaraStackGraphUtilities::AddScriptModuleToStack(UpdateModuleScriptAsset, *ParticleUpdateOutputNode);
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Missing %s"), *UpdateScriptRef.ToString());
						}
					}

					FNiagaraStackGraphUtilities::RelayoutGraph(*Source->NodeGraph);
					NewEmitter->bInterpolatedSpawning = true;
					NewEmitter->SpawnScriptProps.Script->SetUsage(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated);
				}
			}
		}
	}
	
	return NewEmitter;
}

#undef LOCTEXT_NAMESPACE
