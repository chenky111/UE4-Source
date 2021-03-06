// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CaptureTab/SMediaFrameworkCaptureOutputWidget.h"

#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Visibility.h"
#include "LevelEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "SlateOptMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Textures/SlateIcon.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SMediaImage.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

#include "Editor.h"
#include "ILevelViewport.h"
#include "LevelEditor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MediaCapture.h"

#define LOCTEXT_NAMESPACE "MediaFrameworkUtilities"

/*
 * MediaFrameworkUtilities namespace
 */
namespace MediaFrameworkUtilities
{
	const float Padding = 4.f;
	const float ViewportBoxDesiredSizeY = 200.f;
	float GetWidthOverride(TWeakObjectPtr<UMediaOutput> MediaOutput)
	{
		float Ratio = 1.77777777f;
		UMediaOutput* MediaOutputPtr = MediaOutput.Get();
		if (MediaOutputPtr)
		{
			FIntPoint TargetSize = MediaOutputPtr->GetRequestedSize();
			Ratio = (float)TargetSize.X / (float)TargetSize.Y;
		}
		return ViewportBoxDesiredSizeY * Ratio;
	}

	/*
	 * FMediaFrameworkCaptureLevelEditorViewportClient
	 * Like FLevelEditorViewportClient but always use the PlayWorld if available. Do not support bSimulated
	 */
	class FMediaFrameworkCaptureLevelEditorViewportClient : public FLevelEditorViewportClient
	{
	private:
		using Super = FLevelEditorViewportClient;

	public:
		FMediaFrameworkCaptureLevelEditorViewportClient(const TSharedPtr<class SLevelViewport>& InLevelViewport, EViewModeIndex InViewModeIndex)
			: FLevelEditorViewportClient(InLevelViewport)
			, ViewModeIndex(InViewModeIndex)
		{}
		~FMediaFrameworkCaptureLevelEditorViewportClient()
		{
			if (GEditor)
			{
				FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
				if (PIEWorldContext)
				{
					RemoveReferenceToWorldContext(*PIEWorldContext);
				}
			}
		}

		virtual UWorld* GetWorld() const override
		{
			if (GEditor->PlayWorld)
			{
				return GEditor->PlayWorld;
			}
			return Super::GetWorld();
		}

		void SetPIE(bool bInIsPIE)
		{
			FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
			if (bInIsPIE)
			{
				if (PIEWorldContext)
				{
					RemoveReferenceToWorldContext(GEditor->GetEditorWorldContext());
					SetReferenceToWorldContext(*PIEWorldContext);
				}
			}
			else
			{
				if (PIEWorldContext)
				{
					RemoveReferenceToWorldContext(*PIEWorldContext);
				}
				SetReferenceToWorldContext(GEditor->GetEditorWorldContext());
			}
			SetViewMode(ViewModeIndex);
			SetRealtime(true, false);
			SetRealtime(true, true); // Save that setting for RestoreRealtime
		}

		void AutoSetPIE()
		{
			if (GEditor->PlayWorld)
			{
				SetPIE(true);
			}
			else
			{
				SetViewMode(ViewModeIndex);
				SetRealtime(true, false);
				SetRealtime(true, true); // Save that setting for RestoreRealtime
			}
		}

	private:
		EViewModeIndex ViewModeIndex;
	};
}

/*
 * SMediaFrameworkCaptureOutputWidget implementation
 */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureOutputWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	MediaOutput = InArgs._MediaOutput;
	CaptureOptions = InArgs._CaptureOptions;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SMediaFrameworkCaptureOutputWidget::~SMediaFrameworkCaptureOutputWidget()
{
	StopOutput();
}

void SMediaFrameworkCaptureOutputWidget::StopOutput()
{
	if (MediaCapture.IsValid())
	{
		MediaCapture->StopCapture(false);
		MediaCapture.Reset();
	}
}

TSharedRef<SWidget> SMediaFrameworkCaptureOutputWidget::BuildBaseWidget(TSharedRef<SWidget> InnerWidget, const FString& CaptureType)
{
	const FMargin SourceTextPadding(6.f, 2.f, 0.f, 2.f);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		.FillHeight(1.0f)
		.VAlign(EVerticalAlignment::VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Left)
			[
				SNew(STextBlock)
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
				.Text(this, &SMediaFrameworkCaptureOutputWidget::HandleIconText)
				.ColorAndOpacity(this, &SMediaFrameworkCaptureOutputWidget::HandleIconColorAndOpacity)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.Padding(SourceTextPadding)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::Get().GetFontStyle(TEXT("NormalText")))
				.Text(FText::FromString(CaptureType + TEXT(" - ") + MediaOutput->GetName()))
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.AutoHeight()
		[
			// Live view of the Source
			SNew(SBox)
			.Visibility(EVisibility::HitTestInvisible)
			.WidthOverride(MakeAttributeLambda([this]
			{
				return FOptionalSize(MediaFrameworkUtilities::GetWidthOverride(MediaOutput));
			}))
			.HeightOverride(MediaFrameworkUtilities::ViewportBoxDesiredSizeY)
			[
				InnerWidget
			]
		];
}

FSlateColor SMediaFrameworkCaptureOutputWidget::HandleIconColorAndOpacity() const
{
	FSlateColor Result = FLinearColor::Red;
	if (MediaCapture.IsValid())
	{
		EMediaCaptureState State = MediaCapture->GetState();
		switch (State)
		{
		case EMediaCaptureState::Error:
		case EMediaCaptureState::Stopped:
			Result = FLinearColor::Red;
			break;
		case EMediaCaptureState::Capturing:
			Result = FLinearColor::Green;
			break;
		case EMediaCaptureState::Preparing:
		case EMediaCaptureState::StopRequested:
			Result = FLinearColor::Yellow;
			break;
		}
	}
	return Result;
}

FText SMediaFrameworkCaptureOutputWidget::HandleIconText() const
{
	FText Result = FEditorFontGlyphs::Ban;
	if (MediaCapture.IsValid())
	{
		static FText Video_Slash = FText::FromString(FString(TEXT("\xf4e2")));

		EMediaCaptureState State = MediaCapture->GetState();
		switch (State)
		{
		case EMediaCaptureState::Error:
		case EMediaCaptureState::Stopped:
			Result = Video_Slash;
			break;
		case EMediaCaptureState::StopRequested:
			Result = FEditorFontGlyphs::Exclamation;
			break;
		case EMediaCaptureState::Capturing:
			Result = FEditorFontGlyphs::Video_Camera;
			break;
		case EMediaCaptureState::Preparing:
			Result = FEditorFontGlyphs::Hourglass_O;
			break;
		}
	}
	return Result;
}

bool SMediaFrameworkCaptureOutputWidget::IsValid() const
{
	return MediaOutput.IsValid() && MediaCapture.IsValid();
}

/*
 * SMediaFrameworkCaptureCameraViewportWidget implementation
 */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureCameraViewportWidget::Construct(const FArguments& InArgs)
{
	PreviewActors = InArgs._PreviewActors;
	ViewMode = InArgs._ViewMode;
	CurrentLockCameraIndex = 0;

	SMediaFrameworkCaptureOutputWidget::FArguments BaseArguments;
	BaseArguments._Owner = InArgs._Owner;
	BaseArguments._MediaOutput = InArgs._MediaOutput;
	BaseArguments._CaptureOptions = InArgs._CaptureOptions;
	SMediaFrameworkCaptureOutputWidget::Construct(BaseArguments);

	LevelViewportClient = MakeShareable(new MediaFrameworkUtilities::FMediaFrameworkCaptureLevelEditorViewportClient(TSharedPtr<class SLevelViewport>(), ViewMode));
	{
		// Preview viewports never be a listener
		LevelViewportClient->bSetListenerPosition = false;

		// Default to "game" show flags for camera previews
		LevelViewportClient->EngineShowFlags = FEngineShowFlags(ESFIM_Game);
		LevelViewportClient->LastEngineShowFlags = FEngineShowFlags(ESFIM_Editor);

		LevelViewportClient->ViewportType = LVT_Perspective;
		LevelViewportClient->bDrawAxes = false;
		LevelViewportClient->bDisableInput = true;
		LevelViewportClient->SetAllowCinematicControl(false);
		LevelViewportClient->VisibilityDelegate.BindLambda([] {return true; });
		LevelViewportClient->AutoSetPIE();
	}

	UpdateActivePreviewList(GEditor->PlayWorld != nullptr);
	LevelViewportClient->UpdateViewForLockedActor();

	ViewportWidget = SNew(SViewport)
		.RenderDirectlyToWindow(false)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
		.EnableGammaCorrection(false)		// Scene rendering handles gamma correction
		.EnableBlending(false);

	SceneViewport = MakeShareable(new FSceneViewport(LevelViewportClient.Get(), ViewportWidget));
	{
		LevelViewportClient->Viewport = SceneViewport.Get();
		ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());
	}


	TSharedPtr<SWidget> GridPanelWidget = SNullWidget::NullWidget;
	const int32 CameraNum = PreviewActors.Num();
	if (CameraNum > 1)
	{
		TSharedPtr<SGridPanel> GridPanel = SNew(SGridPanel);
		GridPanelWidget = GridPanel;

		const int32 MaxCameraPerRow = 5;

		int32 CameraIndexY = 0;
		int32 CameraCounter = 0;
		while (CameraCounter < CameraNum)
		{
			for (int32 IndexX = 0; IndexX < MaxCameraPerRow && CameraCounter < CameraNum; ++CameraCounter, ++IndexX)
			{
				GridPanel->AddSlot(IndexX, CameraIndexY)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(FText::FromString(PreviewActors[CameraCounter]->GetActorLabel()))
					.OnClicked(FOnClicked::CreateLambda([this, CameraCounter]()
					{
						SetActorLock(CameraCounter);
						return FReply::Handled();
					}))
					.IsEnabled_Lambda([this, CameraCounter]()
					{
						if (ActivePreviewActors.IsValidIndex(CameraCounter))
						{
							return ActivePreviewActors[CameraCounter].IsValid();
						}

						return false;
					})
				];
			}
			++CameraIndexY;
		}
	}

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(MediaFrameworkUtilities::Padding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				GridPanelWidget.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoHeight()
			[
				BuildBaseWidget(ViewportWidget.ToSharedRef(), TEXT("Viewport Capture"))
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SMediaFrameworkCaptureCameraViewportWidget::~SMediaFrameworkCaptureCameraViewportWidget()
{
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->Viewport = nullptr;
	}
}

void SMediaFrameworkCaptureCameraViewportWidget::StartOutput()
{
	UMediaOutput* MediaOutputPtr = MediaOutput.Get();
	if (MediaOutputPtr && SceneViewport.IsValid())
	{
		MediaCapture.Reset(MediaOutputPtr->CreateMediaCapture());
		if (MediaCapture.IsValid())
		{
			FIntPoint TargetSize = MediaOutputPtr->GetRequestedSize();
			SceneViewport->SetFixedViewportSize(TargetSize.X, TargetSize.Y);
			MediaCapture->CaptureSceneViewport(SceneViewport, CaptureOptions);
		}
	}
}

void SMediaFrameworkCaptureCameraViewportWidget::OnPostPIEStarted()
{
	const bool bIsPIE = true;
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->SetPIE(bIsPIE);
	}

	UpdateActivePreviewList(bIsPIE);
}

void SMediaFrameworkCaptureCameraViewportWidget::OnPrePIEEnded()
{
	const bool bIsPIE = false;
	UpdateActivePreviewList(bIsPIE);
	
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->SetPIE(bIsPIE);
	}
}

void SMediaFrameworkCaptureCameraViewportWidget::SetActorLock(int32 CameraIndex)
{
	CurrentLockCameraIndex = CameraIndex;
	if (ActivePreviewActors.IsValidIndex(CurrentLockCameraIndex))
	{
		LevelViewportClient->SetActorLock(ActivePreviewActors[CurrentLockCameraIndex].Get());
	}
	else
	{
		LevelViewportClient->SetActorLock(nullptr);
	}
}

void SMediaFrameworkCaptureCameraViewportWidget::UpdateActivePreviewList(bool bIsPIE)
{
	if (bIsPIE)
	{
		ActivePreviewActors.Empty();
		for (TWeakObjectPtr<AActor> PreviewActor : PreviewActors)
		{
			AActor* PreviewActorPtr = PreviewActor.Get();
			if (PreviewActorPtr != nullptr)
			{
				const bool bIsAlreadyPIEActor = PreviewActorPtr->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
				if (!bIsAlreadyPIEActor)
				{
					AActor* CounterpartActor = EditorUtilities::GetSimWorldCounterpartActor(PreviewActorPtr);
					if (CounterpartActor != nullptr)
					{
						ActivePreviewActors.Add(CounterpartActor);
					}
				}
				else
				{
					ActivePreviewActors.Add(PreviewActor);
				}
			}
		}
	}
	else
	{
		ActivePreviewActors = PreviewActors;
	}

	SetActorLock(CurrentLockCameraIndex);
}

/*
 * SMediaFrameworkCaptureRenderTargetWidget implementation
 */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureRenderTargetWidget::Construct(const FArguments& InArgs)
{
	RenderTarget = InArgs._RenderTarget;

	SMediaFrameworkCaptureOutputWidget::FArguments BaseArguments;
	BaseArguments._Owner = InArgs._Owner;
	BaseArguments._MediaOutput = InArgs._MediaOutput;
	BaseArguments._CaptureOptions = InArgs._CaptureOptions;
	SMediaFrameworkCaptureOutputWidget::Construct(BaseArguments);

	TSharedRef<SWidget> PictureBox = SNew(SMediaImage, RenderTarget.Get());

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(MediaFrameworkUtilities::Padding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				BuildBaseWidget(PictureBox, TEXT("Render Target Capture"))
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMediaFrameworkCaptureRenderTargetWidget::StartOutput()
{
	UMediaOutput* MediaOutputPtr = MediaOutput.Get();
	UTextureRenderTarget2D* RenderTargetPtr = RenderTarget.Get();
	if (MediaOutputPtr && RenderTargetPtr)
	{
		MediaCapture.Reset(MediaOutputPtr->CreateMediaCapture());
		if (MediaCapture.IsValid())
		{
			MediaCapture->CaptureTextureRenderTarget2D(RenderTargetPtr, CaptureOptions);
		}
	}
}

bool SMediaFrameworkCaptureRenderTargetWidget::IsValid() const
{
	return RenderTarget.IsValid();
}

/*
 * SMediaFrameworkCaptureCurrentViewportWidget implementation
 */
SMediaFrameworkCaptureCurrentViewportWidget::FPreviousViewportClientFlags::FPreviousViewportClientFlags()
	: bRealTime(false)
	, bSetListenerPosition(false)
	, bDrawAxes(false)
	, bDisableInput(false)
	, bAllowCinematicControl(false)
	, EngineShowFlags(ESFIM_Editor)
	, LastEngineShowFlags(ESFIM_Game)
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureCurrentViewportWidget::Construct(const FArguments& InArgs)
{
	ViewMode = InArgs._ViewMode;

	SMediaFrameworkCaptureOutputWidget::FArguments BaseArguments;
	BaseArguments._Owner = InArgs._Owner;
	BaseArguments._MediaOutput = InArgs._MediaOutput;
	BaseArguments._CaptureOptions = InArgs._CaptureOptions;
	SMediaFrameworkCaptureOutputWidget::Construct(BaseArguments);

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(MediaFrameworkUtilities::Padding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				BuildBaseWidget(SNullWidget::NullWidget, TEXT("Current Viewport Capture"))
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SMediaFrameworkCaptureCurrentViewportWidget::~SMediaFrameworkCaptureCurrentViewportWidget()
{
	ShutdownViewport();
}

void SMediaFrameworkCaptureCurrentViewportWidget::StartOutput()
{
	ShutdownViewport();

	UMediaOutput* MediaOutputPtr = MediaOutput.Get();
	if (MediaOutputPtr)
	{
		MediaCapture.Reset(MediaOutputPtr->CreateMediaCapture());
		if (MediaCapture.IsValid())
		{
			TSharedPtr<FSceneViewport> SceneViewport;

			// Is it a "standalone" window
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE)
				{
					UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
					FSlatePlayInEditorInfo& Info = EditorEngine->SlatePlayInEditorMap.FindChecked(Context.ContextHandle);
					if (Info.SlatePlayInEditorWindowViewport.IsValid())
					{
						SceneViewport = Info.SlatePlayInEditorWindowViewport;
					}
				}
			}

			if (!SceneViewport.IsValid())
			{
				// Find a editor viewport
				FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
				TSharedPtr<ILevelViewport> LevelViewportInterface = LevelEditorModule.GetFirstActiveViewport();
				if (LevelViewportInterface.IsValid())
				{
					LevelViewport = LevelViewportInterface;
					SceneViewport = LevelViewportInterface->GetSharedActiveViewport();

					FLevelEditorViewportClient& ViewportClient = LevelViewportInterface->GetLevelViewportClient();

					// Save settings
					ViewportClientFlags.bRealTime = ViewportClient.IsRealtime();
					ViewportClientFlags.bSetListenerPosition = ViewportClient.bSetListenerPosition;
					ViewportClientFlags.bDrawAxes = ViewportClient.bDrawAxes;
					ViewportClientFlags.bDisableInput = ViewportClient.bDisableInput;
					ViewportClientFlags.bAllowCinematicControl = ViewportClient.AllowsCinematicControl();
					ViewportClientFlags.VisibilityDelegate = ViewportClient.VisibilityDelegate;

					// Set settings for recording
					ViewportClient.SetRealtime(true);
					ViewportClient.bSetListenerPosition = false;
					ViewportClient.bDrawAxes = false;
					ViewportClient.bDisableInput = true;
					ViewportClient.SetAllowCinematicControl(false);
					ViewportClient.VisibilityDelegate.BindLambda([] { return true; });
				}
			}

			if (SceneViewport.IsValid())
			{
				GEditor->OnLevelViewportClientListChanged().AddRaw(this, &SMediaFrameworkCaptureCurrentViewportWidget::OnLevelViewportClientListChanged);
				EditorSceneViewport = SceneViewport;
				FIntPoint TargetSize = MediaOutputPtr->GetRequestedSize();
				SceneViewport->SetFixedViewportSize(TargetSize.X, TargetSize.Y);
				MediaCapture->CaptureSceneViewport(SceneViewport, CaptureOptions);
			}
		}
	}
}

void SMediaFrameworkCaptureCurrentViewportWidget::OnLevelViewportClientListChanged()
{
	bool bFound = false;
	TSharedPtr<FSceneViewport> EditorSceneViewportPtr = EditorSceneViewport.Pin();
	if (EditorSceneViewportPtr.IsValid())
	{
		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient->Viewport == EditorSceneViewportPtr.Get())
			{
				bFound = true;
				break;
			}
		}
	}

	if (!bFound)
	{
		ShutdownViewport();
	}
}

void SMediaFrameworkCaptureCurrentViewportWidget::OnPrePIE()
{
	if (MediaCapture.IsValid())
	{
		MediaCapture->StopCapture(false);
	}
	ShutdownViewport();
}

void SMediaFrameworkCaptureCurrentViewportWidget::OnPrePIEEnded()
{
	if (MediaCapture.IsValid())
	{
		MediaCapture->StopCapture(false);
	}
	ShutdownViewport();
}

void SMediaFrameworkCaptureCurrentViewportWidget::ShutdownViewport()
{
	TSharedPtr<FSceneViewport> EditorSceneViewportPin = EditorSceneViewport.Pin();
	if (EditorSceneViewportPin.IsValid() && EditorSceneViewportPin->GetViewportWidget().IsValid())
	{
		EditorSceneViewportPin->SetFixedViewportSize(0, 0);
	}

	GEditor->OnLevelViewportClientListChanged().RemoveAll(this);

	TSharedPtr<ILevelViewport> LevelViewportPin = LevelViewport.Pin();
	if (LevelViewportPin.IsValid() && LevelViewportPin->GetSharedActiveViewport() == EditorSceneViewportPin)
	{
		FLevelEditorViewportClient& ViewportClient = LevelViewportPin->GetLevelViewportClient();

		// Reset settings
		ViewportClient.SetRealtime(ViewportClientFlags.bRealTime);
		ViewportClient.bSetListenerPosition = ViewportClientFlags.bSetListenerPosition;
		ViewportClient.bDrawAxes = ViewportClientFlags.bDrawAxes;
		ViewportClient.bDisableInput = ViewportClientFlags.bDisableInput;
		ViewportClient.SetAllowCinematicControl(ViewportClientFlags.bAllowCinematicControl);
		ViewportClient.VisibilityDelegate = ViewportClientFlags.VisibilityDelegate;
	}

	LevelViewport.Reset();
	EditorSceneViewport.Reset();
}

#undef LOCTEXT_NAMESPACE
