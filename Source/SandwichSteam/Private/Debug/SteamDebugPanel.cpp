// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Debug/SteamDebugPanel.h"

#if SANDWICHSTEAM_WITH_DEBUG

#include "Core/SteamCoreSubsystem.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Features/Overlay/SteamOverlaySubsystem.h"
#include "Features/User/SteamUserSubsystem.h"
#include "Features/Utility/SteamUtilitySubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamDebugPanel"

namespace SandwichSteam::Debug
{
	namespace
	{
		TArray<FSteamDebugSection>& GetSectionStorage()
		{
			static TArray<FSteamDebugSection> Sections;
			return Sections;
		}

		const FName CoreSectionIds[] = { FName(TEXT("Core")), FName(TEXT("User")), FName(TEXT("Utility")), FName(TEXT("Overlay")) };

		/** Live view of every registered section. */
		class SSteamDebugPanel : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS(SSteamDebugPanel) {}
			SLATE_END_ARGS()

			void Construct(const FArguments& /*InArgs*/, TFunction<UWorld*()> InWorldGetter)
			{
				WorldGetter = MoveTemp(InWorldGetter);

				ChildSlot
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot().Padding(0.f, 0.f, 0.f, 8.f)
					[
						SAssignNew(StatusText, STextBlock)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.62f, 0.68f)))
						.AutoWrapText(true)
					]
					+ SScrollBox::Slot()
					[
						SAssignNew(SectionBox, SVerticalBox)
					]
				];

				Refresh();
			}

			virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
			{
				SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

				TimeSinceRefresh += InDeltaTime;
				if (TimeSinceRefresh >= 0.5f)
				{
					Refresh();
				}
			}

		private:
			struct FView
			{
				FSteamDebugSection Section;
				TSharedPtr<STextBlock> ReportText;
				TSharedPtr<SWrapBox> ActionBox;
				FString ActionSignature;
			};

			void Refresh()
			{
				TimeSinceRefresh = 0.f;
				UWorld* World = WorldGetter ? WorldGetter() : nullptr;

				const USteamCoreSubsystem* Core = FindFeatureSubsystem<USteamCoreSubsystem>(World);
				if (!World)
				{
					StatusText->SetText(LOCTEXT("NoWorld", "No running game with Steam in this process. Play in Standalone and toggle this panel with the console command Steam.Debug.Show (or use the Steam.*.Dump commands). Steam is not available inside Play In Editor."));
				}
				else
				{
					StatusText->SetText(FText::Format(LOCTEXT("WorldStatus", "World: {0}   Steam: {1}   Steam owner: {2}"),
						FText::FromString(World->GetName()),
						Core ? FText::FromString(LexToString(Core->GetSteamState())) : LOCTEXT("NoCore", "no core subsystem"),
						(Core && Core->IsSteamOwner()) ? LOCTEXT("Yes", "yes") : LOCTEXT("No", "no")));
				}

				const TArray<FSteamDebugSection> Sections = GetSections();
				FString Signature;
				for (const FSteamDebugSection& Section : Sections)
				{
					Signature += Section.Id.ToString() + TEXT("|");
				}

				if (Signature != SectionSignature)
				{
					SectionSignature = Signature;
					RebuildSections(Sections);
				}

				for (const TSharedRef<FView>& View : Views)
				{
					// Sections can be re-registered with new lambdas, so always use the current one.
					const FSteamDebugSection* Current = Sections.FindByPredicate([&View](const FSteamDebugSection& S) { return S.Id == View->Section.Id; });
					if (Current)
					{
						View->Section = *Current;
					}

					const FString Report = View->Section.BuildReport ? View->Section.BuildReport(World) : FString();
					View->ReportText->SetText(FText::FromString(Report));

					TArray<FSteamDebugAction> Actions;
					if (View->Section.BuildActions)
					{
						View->Section.BuildActions(World, Actions);
					}

					FString ActionSignature;
					for (const FSteamDebugAction& Action : Actions)
					{
						ActionSignature += Action.Label.ToString() + TEXT("|");
					}

					// Buttons are only rebuilt when the set changes, so a click is never lost to a rebuild.
					if (ActionSignature != View->ActionSignature)
					{
						View->ActionSignature = ActionSignature;
						View->ActionBox->ClearChildren();
						for (const FSteamDebugAction& Action : Actions)
						{
							View->ActionBox->AddSlot().Padding(0.f, 0.f, 6.f, 4.f)
							[
								SNew(SButton)
								.ContentPadding(FMargin(8.f, 3.f))
								.ToolTipText(Action.ToolTip)
								.OnClicked_Lambda([Execute = Action.Execute, Getter = WorldGetter]()
								{
									if (Execute)
									{
										Execute(Getter ? Getter() : nullptr);
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Text(Action.Label)
								]
							];
						}
					}
				}
			}

			void RebuildSections(const TArray<FSteamDebugSection>& Sections)
			{
				Views.Reset();
				SectionBox->ClearChildren();

				for (const FSteamDebugSection& Section : Sections)
				{
					const TSharedRef<FView> View = MakeShared<FView>();
					View->Section = Section;
					Views.Add(View);

					SectionBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 3.f)
						[
							SNew(STextBlock)
							.Text(Section.Title)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.75f, 1.f)))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(View->ReportText, STextBlock)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
						[
							SAssignNew(View->ActionBox, SWrapBox).UseAllottedSize(true)
						]
					];
				}
			}

			TFunction<UWorld*()> WorldGetter;
			TSharedPtr<STextBlock> StatusText;
			TSharedPtr<SVerticalBox> SectionBox;
			TArray<TSharedRef<FView>> Views;
			FString SectionSignature = TEXT("<none>");
			float TimeSinceRefresh = 0.f;
		};

		TWeakObjectPtr<UGameViewportClient> OverlayViewport;
		TWeakObjectPtr<APlayerController> OverlayController;
		TSharedPtr<SWidget> OverlayWidget;

		FString ReportCore(UWorld* World)
		{
			const USteamCoreSubsystem* Core = FindFeatureSubsystem<USteamCoreSubsystem>(World);
			return Core ? Core->BuildDebugString() : GetFeatureMissingText();
		}

		FString ReportUser(UWorld* World)
		{
			const USteamUserSubsystem* User = FindFeatureSubsystem<USteamUserSubsystem>(World);
			return User ? User->BuildDebugString() : GetFeatureMissingText();
		}

		FString ReportUtility(UWorld* World)
		{
			const USteamUtilitySubsystem* Utility = FindFeatureSubsystem<USteamUtilitySubsystem>(World);
			return Utility ? Utility->BuildDebugString() : GetFeatureMissingText();
		}

		FString ReportOverlay(UWorld* World)
		{
			const USteamOverlaySubsystem* Overlay = FindFeatureSubsystem<USteamOverlaySubsystem>(World);
			return Overlay ? Overlay->BuildDebugString() : GetFeatureMissingText();
		}
	}

	void RegisterSection(FSteamDebugSection Section)
	{
		check(IsInGameThread());
		TArray<FSteamDebugSection>& Sections = GetSectionStorage();
		Sections.RemoveAll([&Section](const FSteamDebugSection& Existing) { return Existing.Id == Section.Id; });
		Sections.Add(MoveTemp(Section));
		Sections.StableSort([](const FSteamDebugSection& A, const FSteamDebugSection& B) { return A.Order < B.Order; });
	}

	void UnregisterSection(FName Id)
	{
		GetSectionStorage().RemoveAll([Id](const FSteamDebugSection& Existing) { return Existing.Id == Id; });
	}

	TArray<FSteamDebugSection> GetSections()
	{
		return GetSectionStorage();
	}

	FString GetFeatureMissingText()
	{
		return TEXT("Not available in this world: the feature is disabled in the settings, its module was removed, or no game is running.");
	}

	UWorld* FindSteamOwnerWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		UWorld* Fallback = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || (Context.WorldType != EWorldType::Game && Context.WorldType != EWorldType::PIE))
			{
				continue;
			}

			const USteamCoreSubsystem* Core = FindFeatureSubsystem<USteamCoreSubsystem>(World);
			if (!Core)
			{
				continue;
			}

			if (Core->IsSteamOwner())
			{
				return World;
			}

			if (!Fallback)
			{
				Fallback = World;
			}
		}
		return Fallback;
	}

	TSharedRef<SWidget> CreateDebugPanel(TFunction<UWorld*()> WorldGetter)
	{
		return SNew(SSteamDebugPanel, MoveTemp(WorldGetter));
	}

	void HideOverlay()
	{
		UGameViewportClient* Viewport = OverlayViewport.Get();
		if (Viewport && OverlayWidget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(OverlayWidget.ToSharedRef());
		}

		if (APlayerController* Controller = OverlayController.Get())
		{
			Controller->SetInputMode(FInputModeGameOnly());
			Controller->bShowMouseCursor = false;
		}

		OverlayWidget.Reset();
		OverlayViewport.Reset();
		OverlayController.Reset();
	}

	void ToggleOverlay(UWorld* World)
	{
		if (OverlayWidget.IsValid())
		{
			HideOverlay();
			return;
		}

		UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
		if (!Viewport)
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam.Debug.Show needs a game viewport. Run it in Standalone or PIE."));
			return;
		}

		const TWeakObjectPtr<UWorld> WeakWorld(World);
		OverlayWidget = SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(FMargin(40.f))
			[
				SNew(SBox)
				.WidthOverride(720.f)
				.HeightOverride(520.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.92f))
					.Padding(16.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("OverlayTitle", "Sandwich Steam - Debug"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.ContentPadding(FMargin(10.f, 4.f))
								.OnClicked_Lambda([]()
								{
									HideOverlay();
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Text(LOCTEXT("Close", "Close"))
								]
							]
						]
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							CreateDebugPanel([WeakWorld]() { return WeakWorld.Get(); })
						]
					]
				]
			];

		Viewport->AddViewportWidgetContent(OverlayWidget.ToSharedRef(), 1000);
		OverlayViewport = Viewport;

		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			Controller->SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
			Controller->bShowMouseCursor = true;
			OverlayController = Controller;
		}
	}

	void RegisterCoreSections()
	{
		FSteamDebugSection Section;

		Section.Id = TEXT("Core");
		Section.Title = LOCTEXT("CoreTitle", "Core");
		Section.Order = 0;
		Section.BuildReport = &ReportCore;
		Section.BuildActions = [](UWorld*, TArray<FSteamDebugAction>& OutActions)
		{
			FSteamDebugAction ThreadCheck;
			ThreadCheck.Label = LOCTEXT("ThreadCheck", "Thread check");
			ThreadCheck.ToolTip = LOCTEXT("ThreadCheckTip", "Sends a harmless Steam request and logs the thread of the raw callback.");
			ThreadCheck.Execute = [](UWorld* World)
			{
				if (USteamCoreSubsystem* Core = FindFeatureSubsystem<USteamCoreSubsystem>(World))
				{
					Core->RunThreadCheck();
				}
			};
			OutActions.Add(MoveTemp(ThreadCheck));
		};
		RegisterSection(Section);

		Section = FSteamDebugSection();
		Section.Id = TEXT("User");
		Section.Title = LOCTEXT("UserTitle", "User");
		Section.Order = 10;
		Section.BuildReport = &ReportUser;
		RegisterSection(Section);

		Section = FSteamDebugSection();
		Section.Id = TEXT("Utility");
		Section.Title = LOCTEXT("UtilityTitle", "Utility");
		Section.Order = 20;
		Section.BuildReport = &ReportUtility;
		RegisterSection(Section);

		Section = FSteamDebugSection();
		Section.Id = TEXT("Overlay");
		Section.Title = LOCTEXT("OverlayTitleSection", "Overlay");
		Section.Order = 30;
		Section.BuildReport = &ReportOverlay;
		RegisterSection(Section);
	}

	void UnregisterCoreSections()
	{
		for (const FName& Id : CoreSectionIds)
		{
			UnregisterSection(Id);
		}
	}
}

#undef LOCTEXT_NAMESPACE

#endif // SANDWICHSTEAM_WITH_DEBUG
