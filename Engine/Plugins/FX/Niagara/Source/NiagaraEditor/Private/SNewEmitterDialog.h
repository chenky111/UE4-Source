// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "AssetData.h"
#include "IAssetTypeActions.h"

class SNiagaraPrototypeAssetPicker;

/** A window which allows the user to select additional content to add to the currently loaded project. */
class SNewEmitterDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SNewEmitterDialog)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool GetUserConfirmedSelection() const;

	TOptional<FAssetData> GetSelectedEmitterAsset();

private:
	void OnAssetPickerAssetSelected(const FAssetData& SelectedAsset);

	ECheckBoxState GetCreateFromPrototypeCheckBoxState() const;

	void CreateFromPrototypeCheckBoxStateChanged(ECheckBoxState InCheckBoxState);

	ECheckBoxState GetCreateFromOtherEmitterCheckBoxState() const;

	void CreateFromOtherEmitterCheckBoxStateChanged(ECheckBoxState InCheckBoxState);

	ECheckBoxState GetCreateEmptyCheckBoxState() const;

	void CreateEmptyCheckBoxStateChanged(ECheckBoxState InCheckBoxState);

	EVisibility GetPrototypeAssetPickerVisibility() const;

	EVisibility GetAssetPickerVisibility() const;

	bool IsOkButtonEnabled() const;

	FReply OnOkButtonClicked();

	FReply OnCancelButtonClicked();

private:
	bool bUserConfirmedSelection;
	bool bCreateFromPrototype;
	bool bCreateFromOtherEmitter;
	bool bCreateEmpty;

	TSharedPtr<SNiagaraPrototypeAssetPicker> PrototypeAssetPicker;

	TOptional<FAssetData> AssetPickerSelectedAsset;

	TOptional<FAssetData> SelectedEmitterAsset;
};