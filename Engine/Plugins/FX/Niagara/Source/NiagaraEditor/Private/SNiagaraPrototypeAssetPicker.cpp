// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraPrototypeAssetPicker.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorStyle.h"

#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "AssetThumbnail.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SNiagaraAssetSelector"

void SNiagaraPrototypeAssetPicker::Construct(const FArguments& InArgs, UClass* AssetClass)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> EmitterAssets;
	AssetRegistryModule.Get().GetAssetsByClass(AssetClass->GetFName(), EmitterAssets);

	NiagaraPluginCategory = LOCTEXT("NiagaraCategory", "Engine (Niagara Plugin)");
	ProjectCategory = LOCTEXT("ProjectCategory", "Project");

	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(24));
		
	TArray<FAssetData> EmittersToShow;
	for (const FAssetData& EmitterAsset : EmitterAssets)
	{
		bool bShowEmitter = false;
		EmitterAsset.GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraEmitter, bIsPrototypeAsset), bShowEmitter);
		if (bShowEmitter)
		{
			EmittersToShow.Add(EmitterAsset);
		}
	}

	ChildSlot
	[
		SAssignNew(ItemSelector, SNiagaraAssetItemSelector)
		.Items(EmittersToShow)
		.OnGetCategoriesForItem(this, &SNiagaraPrototypeAssetPicker::OnGetCategoriesForItem)
		.OnCompareCategoriesForEquality(this, &SNiagaraPrototypeAssetPicker::OnCompareCategoriesForEquality)
		.OnCompareCategoriesForSorting(this, &SNiagaraPrototypeAssetPicker::OnCompareCategoriesForSorting)
		.OnCompareItemsForSorting(this, &SNiagaraPrototypeAssetPicker::OnCompareItemsForSorting)
		.OnDoesItemMatchFilterText(this, &SNiagaraPrototypeAssetPicker::OnDoesItemMatchFilterText)
		.OnGenerateWidgetForCategory(this, &SNiagaraPrototypeAssetPicker::OnGenerateWidgetForCategory)
		.OnGenerateWidgetForItem(this, &SNiagaraPrototypeAssetPicker::OnGenerateWidgetForItem)
	];
}

void SNiagaraPrototypeAssetPicker::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	AssetThumbnailPool->Tick(InDeltaTime);
}

TArray<FAssetData> SNiagaraPrototypeAssetPicker::GetSelectedAssets() const
{
	return ItemSelector->GetSelectedItems();
}

TArray<FText> SNiagaraPrototypeAssetPicker::OnGetCategoriesForItem(const FAssetData& Item)
{
	TArray<FText> Categories;
	TArray<FString> AssetPathParts;
	Item.ObjectPath.ToString().ParseIntoArray(AssetPathParts, TEXT("/"));
	if (AssetPathParts.Num() > 0)
	{
		if (AssetPathParts[0] == TEXT("Niagara"))
		{
			Categories.Add(LOCTEXT("NiagaraCategory", "Engine (Niagara Plugin)"));
		}
		else if(AssetPathParts[0] == TEXT("Game"))
		{
			Categories.Add(LOCTEXT("ProjectCategory", "Project"));
		}
		else
		{
			Categories.Add(FText::Format(LOCTEXT("OtherPluginFormat", "Plugin - {0}"), FText::FromString(AssetPathParts[0])));
		}
	}
	return Categories;
}

bool SNiagaraPrototypeAssetPicker::OnCompareCategoriesForEquality(const FText& CategoryA, const FText& CategoryB) const
{
	return CategoryA.CompareTo(CategoryB) == 0;
}

bool SNiagaraPrototypeAssetPicker::OnCompareCategoriesForSorting(const FText& CategoryA, const FText& CategoryB) const
{
	int32 CompareResult = CategoryA.CompareTo(CategoryB);
	if (CompareResult != 0)
	{
		// Niagara plugin first.
		if (CategoryA.CompareTo(NiagaraPluginCategory) == 0)
		{
			return true;
		}
		if (CategoryB.CompareTo(NiagaraPluginCategory) == 0)
		{
			return false;
		}

		// Project last
		if (CategoryA.CompareTo(ProjectCategory) == 0)
		{
			return false;
		}
		if (CategoryB.CompareTo(ProjectCategory) == 0)
		{
			return true;
		}
	}
	// Otherwise just return the actual result.
	return CompareResult == -1;
}

bool SNiagaraPrototypeAssetPicker::OnCompareItemsForSorting(const FAssetData& ItemA, const FAssetData& ItemB) const
{
	return ItemA.AssetName.ToString().Compare(ItemB.AssetName.ToString()) == -1;
}

bool SNiagaraPrototypeAssetPicker::OnDoesItemMatchFilterText(const FText& FilterText, const FAssetData& Item)
{
	return Item.AssetName.ToString().Contains(FilterText.ToString());
}

TSharedRef<SWidget> SNiagaraPrototypeAssetPicker::OnGenerateWidgetForCategory(const FText& Category)
{
	return SNew(SBox)
		.Padding(FMargin(5, 5, 5, 3))
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.AssetPickerAssetCategoryText")
			.Text(Category)
		];
}

const int32 ThumbnailSize = 96;

TSharedRef<SWidget> SNiagaraPrototypeAssetPicker::OnGenerateWidgetForItem(const FAssetData& Item)
{
	TSharedRef<FAssetThumbnail> AssetThumbnail = MakeShared<FAssetThumbnail>(Item, ThumbnailSize, ThumbnailSize, AssetThumbnailPool);
	FAssetThumbnailConfig ThumbnailConfig;
	ThumbnailConfig.bAllowFadeIn = true;

	FText AssetDescription;
	Item.GetTagValue(GET_MEMBER_NAME_CHECKED(UNiagaraEmitter, PrototypeAssetDescription), AssetDescription);

	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(15, 3, 5, 5)
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.AssetPickerAssetNameText")
			.Text(FText::FromString(FName::NameToDisplayString(Item.AssetName.ToString(), false)))
		]
		+ SVerticalBox::Slot()
		.Padding(15, 0, 5, 5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 10, 0)
			[
				SNew(SBox)
				.WidthOverride(ThumbnailSize)
				.HeightOverride(ThumbnailSize)
				[
					AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
				]
			]
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(AssetDescription)
				.AutoWrapText(true)
			]
		];
}
