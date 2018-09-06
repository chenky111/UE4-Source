// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNewEmitterDialog.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorStyle.h"
#include "SNiagaraPrototypeAssetPicker.h"

#include "AssetData.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "EditorStyleSet.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

#define LOCTEXT_NAMESPACE "SNewEmitterDialog"

void SNewEmitterDialog::Construct(const FArguments& InArgs)
{
	bUserConfirmedSelection = false;
	bCreateFromPrototype = true;
	bCreateFromOtherEmitter = false;
	bCreateEmpty = false;

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.SelectionMode = ESelectionMode::SingleToggle;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SNewEmitterDialog::OnAssetPickerAssetSelected);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.Filter.ClassNames.Add(UNiagaraEmitter::StaticClass()->GetFName());

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TSharedRef<SWidget> AssetPicker = ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("NewEmitterDialogTitle", "Pick a starting point for your emitter"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(420.0f, 500.0f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			// Creation mode radio buttons.
			SNew(SVerticalBox)

			// Mode label
			+ SVerticalBox::Slot()
			.Padding(0, 7, 0, 0)
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				[
					SNew(STextBlock)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialogHeaderText")
					.Text(LOCTEXT("OptionsLabel", "Options"))
				]
			]

			// Creation mode radio buttons.
			+ SVerticalBox::Slot()
			.Padding(0, 5, 0, 5)
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				[
					
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(7))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.Padding(0, 0, 0, 5)
						[
							SNew(SCheckBox)
							.Style(FCoreStyle::Get(), "RadioButton")
							.IsChecked(this, &SNewEmitterDialog::GetCreateFromPrototypeCheckBoxState)
							.OnCheckStateChanged(this, &SNewEmitterDialog::CreateFromPrototypeCheckBoxStateChanged)
							.Content()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CreateFromPrototypeLabel", "Create a new emitter from an emitter prototype"))
								.AutoWrapText(true)
							]
						]
						
						+ SVerticalBox::Slot()
						.Padding(0, 0, 0, 5)
						[
							SNew(SCheckBox)
							.Style(FCoreStyle::Get(), "RadioButton")
							.IsChecked(this, &SNewEmitterDialog::GetCreateFromOtherEmitterCheckBoxState)
							.OnCheckStateChanged(this, &SNewEmitterDialog::CreateFromOtherEmitterCheckBoxStateChanged)
							.Content()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CreateFromOtherEmitterLabel", "Copy an existing emitter from your project content"))
								.AutoWrapText(true)
							]
						]

						+ SVerticalBox::Slot()
						[
							SNew(SCheckBox)
							.Style(FCoreStyle::Get(), "RadioButton")
							.IsChecked(this, &SNewEmitterDialog::GetCreateEmptyCheckBoxState)
							.OnCheckStateChanged(this, &SNewEmitterDialog::CreateEmptyCheckBoxStateChanged)
							.Content()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CreateEmptyLabel", "Create an empty emitter with no modules or renderers (Advanced)"))
								.AutoWrapText(true)
							]
						]
					]
				]
			]

			// Asset pickers label
			+ SVerticalBox::Slot()
			.Padding(0, 5, 0, 0)
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				[
					SNew(STextBlock)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialogHeaderText")
					.Text(LOCTEXT("EmitterLabel", "Emitters"))
				]
			]

			// Asset pickers
			+ SVerticalBox::Slot()
			.Padding(0, 5, 0, 5)
			[
				SNew(SBox)
				.Padding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(7))
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SAssignNew(PrototypeAssetPicker, SNiagaraPrototypeAssetPicker, UNiagaraEmitter::StaticClass())
							.Visibility(this, &SNewEmitterDialog::GetPrototypeAssetPickerVisibility)
						]
						+ SOverlay::Slot()
						[
							SNew(SBox)
							.Visibility(this, &SNewEmitterDialog::GetAssetPickerVisibility)
							[
								AssetPicker
							]
						]
					]
				]
			]

			// OK/Cancel buttons
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0, 5, 0, 5)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked(this, &SNewEmitterDialog::OnOkButtonClicked)
					.IsEnabled(this, &SNewEmitterDialog::IsOkButtonEnabled)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SNewEmitterDialog::OnCancelButtonClicked)
				]
			]
		]);
}

bool SNewEmitterDialog::GetUserConfirmedSelection() const
{
	return bUserConfirmedSelection;
}

TOptional<FAssetData> SNewEmitterDialog::GetSelectedEmitterAsset()
{
	return SelectedEmitterAsset;
}

void SNewEmitterDialog::OnAssetPickerAssetSelected(const FAssetData& SelectedAsset)
{
	if (SelectedAsset.IsValid())
	{
		AssetPickerSelectedAsset = SelectedAsset;
	}
	else
	{
		AssetPickerSelectedAsset.Reset();
	}
}

ECheckBoxState SNewEmitterDialog::GetCreateFromPrototypeCheckBoxState() const
{
	return bCreateFromPrototype ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNewEmitterDialog::CreateFromPrototypeCheckBoxStateChanged(ECheckBoxState InCheckBoxState)
{
	if (InCheckBoxState == ECheckBoxState::Checked)
	{
		bCreateFromPrototype = true;
		bCreateFromOtherEmitter = false;
		bCreateEmpty = false;
	}
}

ECheckBoxState SNewEmitterDialog::GetCreateFromOtherEmitterCheckBoxState() const
{
	return bCreateFromOtherEmitter ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNewEmitterDialog::CreateFromOtherEmitterCheckBoxStateChanged(ECheckBoxState InCheckBoxState)
{
	if (InCheckBoxState == ECheckBoxState::Checked)
	{
		bCreateFromOtherEmitter = true;
		bCreateFromPrototype = false;
		bCreateEmpty = false;
	}
}

ECheckBoxState SNewEmitterDialog::GetCreateEmptyCheckBoxState() const
{
	return bCreateEmpty ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void  SNewEmitterDialog::CreateEmptyCheckBoxStateChanged(ECheckBoxState InCheckBoxState)
{
	if (InCheckBoxState == ECheckBoxState::Checked)
	{
		bCreateEmpty = true;
		bCreateFromPrototype = false;
		bCreateFromOtherEmitter = false;
	}
}

EVisibility SNewEmitterDialog::GetPrototypeAssetPickerVisibility() const
{
	return bCreateFromPrototype ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNewEmitterDialog::GetAssetPickerVisibility() const
{
	return bCreateFromOtherEmitter ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SNewEmitterDialog::IsOkButtonEnabled() const
{
	return
		(bCreateFromPrototype && PrototypeAssetPicker.IsValid() && PrototypeAssetPicker->GetSelectedAssets().Num() > 0) ||
		(bCreateFromOtherEmitter && AssetPickerSelectedAsset.IsSet()) ||
		bCreateEmpty;
}

FReply SNewEmitterDialog::OnOkButtonClicked()
{
	if (bCreateFromPrototype)
	{
		TArray<FAssetData> SelectedAssets = PrototypeAssetPicker->GetSelectedAssets();
		if (SelectedAssets.Num() > 0)
		{
			SelectedEmitterAsset = SelectedAssets[0];
		}
	}
	else if (bCreateFromOtherEmitter)
	{
		SelectedEmitterAsset = AssetPickerSelectedAsset;
	}
	else
	{
		SelectedEmitterAsset.Reset();
	}

	bUserConfirmedSelection = true;
	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SNewEmitterDialog::OnCancelButtonClicked()
{
	SelectedEmitterAsset.Reset();
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE