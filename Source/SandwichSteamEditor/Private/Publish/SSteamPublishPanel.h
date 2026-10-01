// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/SteamPublishHistory.h"
#include "Publish/SteamPublishJob.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class SVerticalBox;
template <typename OptionType> class SComboBox;

/**
 * Tools > Steam Publish. Three pages: Publish (branch, dry run, publish, steps, progress, live log), Setup (depots,
 * branches and packaging in a details view, saved as you edit - SteamCMD path and account are on the dashboard's SteamCMD
 * page instead) and History (the uploads of this project). All work is done by FSteamPublishJob.
 */
class SSteamPublishPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSteamPublishPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSteamPublishPanel() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** False while a publish runs and the user declines to cancel it. Cancels the run when they accept. */
	bool CanCloseTab();

private:
	struct FLogLine
	{
		FString Text;
		ESteamLogSeverity Severity = ESteamLogSeverity::Info;
	};

	TSharedRef<SWidget> BuildPublishPage();
	TSharedRef<SWidget> BuildSetupPage();
	TSharedRef<SWidget> BuildHistoryPage();
	TSharedRef<SWidget> MakePageToggle(int32 Page, const FText& Label);

	void RefreshBranches();
	void RebuildChecks();
	void RefreshHistory();

	void OnPublishClicked(bool bDryRun, bool bSkipUpload = false);
	void StartJob(const FSteamPublishOptions& Options);
	bool IsBranchSetLive(const FString& Branch) const;

	/** Shows what the run will do (steps, build settings, depots, pre-flight warnings) and returns true when the user confirms. */
	bool ConfirmRun(const FSteamPublishOptions& Options) const;

	void HandleLog(const FString& Line, ESteamLogSeverity Severity);
	void HandleStepChanged(ESteamPublishStepId Step, ESteamPublishStepState State);
	void HandleProgress(float Progress);
	void HandleStatus(const FText& Status);
	void HandleGuardRequested(bool bMobile);
	void HandleFinished(const FSteamPublishResult& Result);
	void HandleObjectChanged(UObject* Object, FPropertyChangedEvent& Event);

	bool IsRunning() const { return Job.IsValid() && Job->IsRunning(); }

	TSharedPtr<FSteamPublishJob> Job;

	int32 ActivePage = 0;
	TSharedPtr<class SWidgetSwitcher> Pages;

	// Publish page
	TArray<TSharedPtr<FString>> BranchOptions;
	TSharedPtr<FString> SelectedBranch;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> BranchCombo;
	bool bSkipPackaging = false;
	bool bDisableLiveCoding = true;
	TSharedPtr<SVerticalBox> ChecksBox;
	TArray<FSteamValidationCheck> Checks;

	TMap<ESteamPublishStepId, ESteamPublishStepState> StepStates;
	FText StatusText;
	ESteamCheckSeverity StatusSeverity = ESteamCheckSeverity::Info;
	float ProgressValue = -1.f;

	TArray<TSharedPtr<FLogLine>> LogLines;
	TSharedPtr<SListView<TSharedPtr<FLogLine>>> LogList;
	bool bLogDirty = false;

	// History page
	TArray<TSharedPtr<FSteamPublishHistoryEntry>> HistoryEntries;
	TSharedPtr<SListView<TSharedPtr<FSteamPublishHistoryEntry>>> HistoryList;

	TSharedPtr<IDetailsView> SharedDetails;
	FDelegateHandle ObjectChangedHandle;
};

namespace SandwichSteam::Editor
{
	/** Registers and removes the Steam Publish tab (Tools menu entry calls OpenPublishTab). */
	void RegisterPublishTab();
	void UnregisterPublishTab();
	void OpenPublishTab();

	/** Registers the dashboard's "Publish" page: builds SSteamPublishPanel and reuses its own close-tab confirmation while a job runs. */
	void RegisterPublishDashboardPage();
}
