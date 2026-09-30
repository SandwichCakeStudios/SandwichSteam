// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamStatsTestPage.h"
#include "Data/SteamAppDefinition.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamAchievementsLibrary.h"
#include "SteamAchievementsSubsystem.h"
#include "SteamStatsLibrary.h"
#include "SteamStatsSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestStats"

void SSandwichSteamStatsTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Needs a Steam App Definition in the Sandwich Steam settings for stats and tags.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Stats", "Stats")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("StatsActive", "Feature active / ready"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamStatsSubsystem* Stats = GetStats();
			return Stats ? FText::Format(LOCTEXT("StatsActiveFmt", "{0} / {1}"), SandwichSteamTest::BoolText(Stats->IsFeatureActive()), SandwichSteamTest::BoolText(Stats->AreStatsReady())) : LOCTEXT("NoStats", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("StorePending", "Upload pending"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamStatsSubsystem* Stats = GetStats();
			return Stats ? SandwichSteamTest::BoolText(Stats->IsStorePending()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildStatsText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("AddOne", "+1 to every stat"), FOnClicked::CreateSP(this, &SSandwichSteamStatsTestPage::OnAddToStats)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("StoreNow", "Store now"), FOnClicked::CreateSP(this, &SSandwichSteamStatsTestPage::OnStoreNow)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Achievements", "Achievements")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("AchActive", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamAchievementsSubsystem* Achievements = GetAchievements();
			return Achievements ? SandwichSteamTest::BoolText(Achievements->IsFeatureActive()) : LOCTEXT("NoAch", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildAchievementsText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Refresh", "Refresh list"), FOnClicked::CreateSP(this, &SSandwichSteamStatsTestPage::OnRefreshAchievements)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("UnlockNext", "Unlock next locked"), FOnClicked::CreateSP(this, &SSandwichSteamStatsTestPage::OnUnlockNext)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ClearLast", "Clear last unlocked"), FOnClicked::CreateSP(this, &SSandwichSteamStatsTestPage::OnClearLast)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Percent", "Load global %"), FOnClicked::CreateSP(this, &SSandwichSteamStatsTestPage::OnRequestPercentages)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ByName", "By-name check"), FOnClicked::CreateSP(this, &SSandwichSteamStatsTestPage::OnByNameCheck)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)))
			.Text_Lambda([this] { return Status; })
		]
	];
}

USteamStatsSubsystem* SSandwichSteamStatsTestPage::GetStats() const
{
	return USteamStatsSubsystem::Get(WorldContext.Get());
}

USteamAchievementsSubsystem* SSandwichSteamStatsTestPage::GetAchievements() const
{
	return USteamAchievementsSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamStatsTestPage::BuildStatsText() const
{
	const USteamStatsSubsystem* Stats = GetStats();
	const USteamAppDefinition* Definition = Stats ? Stats->GetDefinition() : nullptr;
	if (!Definition)
	{
		return LOCTEXT("NoDefinition", "No Steam App Definition assigned (Project Settings > Plugins > Sandwich Steam).");
	}

	FString Text;
	for (const FSteamStatDef& Def : Definition->Stats)
	{
		FString Value = TEXT("?");
		if (Def.Type == ESteamStatType::Int)
		{
			int32 IntValue = 0;
			if (Stats->GetInt(Def.ApiName, IntValue).IsSuccess())
			{
				Value = FString::FromInt(IntValue);
			}
		}
		else
		{
			float FloatValue = 0.f;
			if (Stats->GetFloat(Def.ApiName, FloatValue).IsSuccess())
			{
				Value = FString::SanitizeFloat(FloatValue);
			}
		}
		Text += FString::Printf(TEXT("%s = %s\n"), *Def.ApiName.ToString(), *Value);
	}
	return Text.IsEmpty() ? LOCTEXT("NoStatRows", "The definition has no stats.") : FText::FromString(Text.TrimEnd());
}

FText SSandwichSteamStatsTestPage::BuildAchievementsText() const
{
	if (Infos.IsEmpty())
	{
		return LOCTEXT("NoInfos", "Press \"Refresh list\" once the stats are ready.");
	}

	FString Text;
	for (const FSteamAchievementInfo& Info : Infos)
	{
		Text += FString::Printf(TEXT("[%s] %s - %s%s\n"), Info.bUnlocked ? TEXT("x") : TEXT(" "), *Info.ApiName.ToString(), *Info.DisplayName,
			Info.ProgressMax > 0 ? *FString::Printf(TEXT(" (%d/%d)"), Info.ProgressCurrent, Info.ProgressMax) : TEXT(""));
	}
	return FText::FromString(Text.TrimEnd());
}

void SSandwichSteamStatsTestPage::ReportResult(const FText& Action, const FSteamResult& Result)
{
	Status = Result.IsSuccess()
		? FText::Format(LOCTEXT("ActionOk", "{0}: ok"), Action)
		: FText::Format(LOCTEXT("ActionFailed", "{0}: {1}"), Action, Result.Message);
}

FReply SSandwichSteamStatsTestPage::OnAddToStats()
{
	USteamStatsSubsystem* Stats = GetStats();
	const USteamAppDefinition* Definition = Stats ? Stats->GetDefinition() : nullptr;
	if (!Definition)
	{
		Status = LOCTEXT("AddNoDefinition", "No Steam App Definition assigned.");
		return FReply::Handled();
	}

	FSteamResult Last = FSteamResult::Success();
	for (const FSteamStatDef& Def : Definition->Stats)
	{
		const FSteamResult Result = Def.Type == ESteamStatType::Int ? Stats->AddInt(Def.ApiName, 1) : Stats->AddFloat(Def.ApiName, 1.f);
		if (!Result.IsSuccess())
		{
			Last = Result;
		}
	}
	ReportResult(LOCTEXT("AddAction", "+1 to every stat"), Last);
	return FReply::Handled();
}

FReply SSandwichSteamStatsTestPage::OnStoreNow()
{
	USteamStatsSubsystem* Stats = GetStats();
	ReportResult(LOCTEXT("StoreAction", "Store now"), Stats ? Stats->StoreStatsNow() : FSteamResult::Failure(FGameplayTag(), LOCTEXT("StoreNoFeature", "Stats feature is not available.")));
	return FReply::Handled();
}

FReply SSandwichSteamStatsTestPage::OnRefreshAchievements()
{
	if (const USteamAchievementsSubsystem* Achievements = GetAchievements())
	{
		Achievements->GetAllInfo(Infos);
		Status = FText::Format(LOCTEXT("Refreshed", "{0} achievement(s)."), FText::AsNumber(Infos.Num()));
	}
	else
	{
		Status = LOCTEXT("NoAchievements", "Achievements feature is not available.");
	}
	return FReply::Handled();
}

FReply SSandwichSteamStatsTestPage::OnUnlockNext()
{
	USteamAchievementsSubsystem* Achievements = GetAchievements();
	if (!Achievements)
	{
		Status = LOCTEXT("UnlockNoFeature", "Achievements feature is not available.");
		return FReply::Handled();
	}

	Achievements->GetAllInfo(Infos);
	const FSteamAchievementInfo* Next = Infos.FindByPredicate([](const FSteamAchievementInfo& Info) { return !Info.bUnlocked; });
	if (!Next)
	{
		Status = LOCTEXT("NothingToUnlock", "Nothing to unlock (all unlocked, or the stats are not ready).");
		return FReply::Handled();
	}

	ReportResult(FText::Format(LOCTEXT("UnlockAction", "Unlock {0}"), FText::FromName(Next->ApiName)), Achievements->Unlock(Next->ApiName));
	return FReply::Handled();
}

FReply SSandwichSteamStatsTestPage::OnClearLast()
{
	USteamAchievementsSubsystem* Achievements = GetAchievements();
	if (!Achievements)
	{
		Status = LOCTEXT("ClearNoFeature", "Achievements feature is not available.");
		return FReply::Handled();
	}

	Achievements->GetAllInfo(Infos);
	const FSteamAchievementInfo* Last = nullptr;
	for (const FSteamAchievementInfo& Info : Infos)
	{
		if (Info.bUnlocked)
		{
			Last = &Info;
		}
	}

	if (!Last)
	{
		Status = LOCTEXT("NothingToClear", "No unlocked achievement to clear.");
		return FReply::Handled();
	}

	ReportResult(FText::Format(LOCTEXT("ClearAction", "Clear {0}"), FText::FromName(Last->ApiName)), Achievements->Clear(Last->ApiName));
	return FReply::Handled();
}

FReply SSandwichSteamStatsTestPage::OnRequestPercentages()
{
	USteamAchievementsSubsystem* Achievements = GetAchievements();
	if (!Achievements)
	{
		Status = LOCTEXT("PercentNoFeature", "Achievements feature is not available.");
		return FReply::Handled();
	}

	const FSteamResult Started = Achievements->RequestGlobalPercentages(FSteamAchievementPercentagesDelegate::CreateSP(this, &SSandwichSteamStatsTestPage::HandlePercentages));
	Status = Started.IsSuccess() ? LOCTEXT("PercentLoading", "Loading global percentages...") : FText::Format(LOCTEXT("PercentFailed", "Request failed: {0}"), Started.Message);
	return FReply::Handled();
}

void SSandwichSteamStatsTestPage::HandlePercentages(const FSteamResult& Result)
{
	if (!Result.IsSuccess())
	{
		Status = FText::Format(LOCTEXT("PercentError", "Global percentages failed: {0}"), Result.Message);
		return;
	}

	FString Text;
	const USteamAchievementsSubsystem* Achievements = GetAchievements();
	if (Achievements)
	{
		for (const FSteamAchievementInfo& Info : Infos)
		{
			float Percent = 0.f;
			if (Achievements->GetGlobalPercent(Info.ApiName, Percent))
			{
				Text += FString::Printf(TEXT("%s %.1f%%  "), *Info.ApiName.ToString(), Percent);
			}
		}
	}
	Status = Text.IsEmpty()
		? LOCTEXT("PercentLoaded", "Global percentages loaded (read them with Get Steam Achievement Global Percent, by tag or by name).")
		: FText::Format(LOCTEXT("PercentValues", "Global percentages: {0}"), FText::FromString(Text));
}

FReply SSandwichSteamStatsTestPage::OnByNameCheck()
{
	// Goes through the Blueprint library (the By Name nodes), not the subsystem, so the nodes themselves are tested.
	const UObject* Context = WorldContext.Get();
	TArray<FString> Lines;

	const USteamStatsSubsystem* Stats = GetStats();
	const USteamAppDefinition* Definition = Stats ? Stats->GetDefinition() : nullptr;
	if (Definition && !Definition->Stats.IsEmpty())
	{
		const FSteamStatDef& Def = Definition->Stats[0];
		FSteamResult Result;
		FString Value;
		if (Def.Type == ESteamStatType::Int)
		{
			int32 IntValue = 0;
			Result = USteamStatsLibrary::GetSteamStatIntByName(Context, Def.ApiName, IntValue);
			Value = FString::FromInt(IntValue);
		}
		else
		{
			float FloatValue = 0.f;
			Result = USteamStatsLibrary::GetSteamStatFloatByName(Context, Def.ApiName, FloatValue);
			Value = FString::SanitizeFloat(FloatValue);
		}
		Lines.Add(FString::Printf(TEXT("Get Stat By Name %s: %s"), *Def.ApiName.ToString(), Result.IsSuccess() ? *Value : *Result.Message.ToString()));
	}
	else
	{
		Lines.Add(TEXT("Stat by name: no stat row to try."));
	}

	if (const USteamAchievementsSubsystem* Achievements = GetAchievements())
	{
		Achievements->GetAllInfo(Infos);
	}
	if (!Infos.IsEmpty())
	{
		const FName Name = Infos[0].ApiName;
		FSteamAchievementInfo Info;
		const FSteamResult InfoResult = USteamAchievementsLibrary::GetSteamAchievementInfoByName(Context, Name, Info);
		const bool bUnlocked = USteamAchievementsLibrary::IsSteamAchievementUnlockedByName(Context, Name);
		Lines.Add(FString::Printf(TEXT("Achievement Info By Name %s: %s, unlocked %s"), *Name.ToString(),
			InfoResult.IsSuccess() ? *Info.DisplayName : *InfoResult.Message.ToString(), bUnlocked ? TEXT("yes") : TEXT("no")));
	}
	else
	{
		Lines.Add(TEXT("Achievement by name: none reported yet (stats not ready?)."));
	}

	// A typo must fail with a message and log one warning (press twice: still one warning).
	FSteamAchievementInfo Unknown;
	const FSteamResult UnknownResult = USteamAchievementsLibrary::GetSteamAchievementInfoByName(Context, TEXT("OST_NoSuchAchievement"), Unknown);
	Lines.Add(FString::Printf(TEXT("Unknown name: %s"), UnknownResult.IsSuccess() ? TEXT("SUCCEEDED (wrong!)") : *UnknownResult.Message.ToString()));

	Status = FText::FromString(FString::Join(Lines, TEXT("\n")));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
