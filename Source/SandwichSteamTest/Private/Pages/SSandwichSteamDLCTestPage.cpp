// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamDLCTestPage.h"
#include "Data/SteamAppDefinition.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamDLCLibrary.h"
#include "SteamDLCSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestDLC"

void SSandwichSteamDLCTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. The buttons use the first DLC row of the Steam App Definition.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("DLC", "DLC")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamDLCSubsystem* DLC = GetDLC();
			return DLC ? SandwichSteamTest::BoolText(DLC->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildListText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Install", "Install first row"), FOnClicked::CreateSP(this, &SSandwichSteamDLCTestPage::OnInstall)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Uninstall", "Uninstall first row"), FOnClicked::CreateSP(this, &SSandwichSteamDLCTestPage::OnUninstall)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Store", "Open store page"), FOnClicked::CreateSP(this, &SSandwichSteamDLCTestPage::OnStore)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ByAppId", "By App ID check"), FOnClicked::CreateSP(this, &SSandwichSteamDLCTestPage::OnByAppIdCheck)) ]
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

USteamDLCSubsystem* SSandwichSteamDLCTestPage::GetDLC() const
{
	return USteamDLCSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamDLCTestPage::BuildListText() const
{
	const USteamDLCSubsystem* DLC = GetDLC();
	if (!DLC)
	{
		return FText::GetEmpty();
	}

	FString Text;
	if (const USteamAppDefinition* Definition = DLC->GetDefinition())
	{
		for (const FSteamDLCDef& Def : Definition->DLC)
		{
			Text += FString::Printf(TEXT("Row [%s] %d: owned %s, installed %s\n"), *Def.Tag.ToString(), Def.AppId,
				DLC->IsOwnedApp(Def.AppId) ? TEXT("yes") : TEXT("no"), DLC->IsInstalledApp(Def.AppId) ? TEXT("yes") : TEXT("no"));
		}
	}

	for (const FSteamDLCInfo& Info : DLC->ListDLC())
	{
		Text += FString::Printf(TEXT("Steam: %d %s (available %s, owned %s, installed %s)\n"), Info.AppId, *Info.Name, Info.bAvailable ? TEXT("yes") : TEXT("no"),
			Info.bOwned ? TEXT("yes") : TEXT("no"), Info.bInstalled ? TEXT("yes") : TEXT("no"));
	}

	return Text.IsEmpty() ? LOCTEXT("None", "No DLC rows and Steam lists no DLC for this App ID.") : FText::FromString(Text.TrimEnd());
}

FReply SSandwichSteamDLCTestPage::OnInstall()
{
	USteamDLCSubsystem* DLC = GetDLC();
	const USteamAppDefinition* Definition = DLC ? DLC->GetDefinition() : nullptr;
	if (!Definition || Definition->DLC.IsEmpty())
	{
		Status = LOCTEXT("NoRow", "Needs the DLC feature and a DLC row in the Steam App Definition.");
		return FReply::Handled();
	}

	const FSteamResult Result = DLC->InstallApp(Definition->DLC[0].AppId);
	Status = Result.IsSuccess() ? LOCTEXT("InstallAsked", "Asked Steam to install it. The DLC Installed event fires when done.") : FText::Format(LOCTEXT("InstallFailed", "Install failed: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamDLCTestPage::OnUninstall()
{
	USteamDLCSubsystem* DLC = GetDLC();
	const USteamAppDefinition* Definition = DLC ? DLC->GetDefinition() : nullptr;
	if (!Definition || Definition->DLC.IsEmpty())
	{
		Status = LOCTEXT("NoRowUninstall", "Needs the DLC feature and a DLC row in the Steam App Definition.");
		return FReply::Handled();
	}

	const FSteamResult Result = DLC->UninstallApp(Definition->DLC[0].AppId);
	Status = Result.IsSuccess() ? LOCTEXT("UninstallAsked", "Asked Steam to uninstall it.") : FText::Format(LOCTEXT("UninstallFailed", "Uninstall failed: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamDLCTestPage::OnStore()
{
	USteamDLCSubsystem* DLC = GetDLC();
	const USteamAppDefinition* Definition = DLC ? DLC->GetDefinition() : nullptr;
	if (!Definition || Definition->DLC.IsEmpty())
	{
		Status = LOCTEXT("NoRowStore", "Needs the DLC feature and a DLC row in the Steam App Definition.");
		return FReply::Handled();
	}

	const FSteamResult Result = DLC->OpenStorePage(Definition->DLC[0].Tag);
	Status = Result.IsSuccess() ? LOCTEXT("StoreOpened", "Store page opened.") : FText::Format(LOCTEXT("StoreFailed", "Could not open it: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamDLCTestPage::OnByAppIdCheck()
{
	// Through the Blueprint library (the By App ID nodes), on the first DLC Steam lists: no App Definition row needed.
	const USteamDLCSubsystem* DLC = GetDLC();
	const TArray<FSteamDLCInfo> List = DLC ? DLC->ListDLC() : TArray<FSteamDLCInfo>();
	const UObject* Context = WorldContext.Get();

	FString Text;
	if (!List.IsEmpty())
	{
		const int32 AppId = List[0].AppId;
		const FSteamDLCProgress Progress = USteamDLCLibrary::GetSteamDLCProgressByAppId(Context, AppId);
		Text = FString::Printf(TEXT("By App ID %d: owned %s, installed %s, downloading %s\n"), AppId,
			USteamDLCLibrary::IsSteamDLCAppOwned(Context, AppId) ? TEXT("yes") : TEXT("no"),
			USteamDLCLibrary::IsSteamDLCInstalledByAppId(Context, AppId) ? TEXT("yes") : TEXT("no"),
			Progress.bDownloading ? TEXT("yes") : TEXT("no"));
	}
	else
	{
		Text = TEXT("Steam lists no DLC for this App ID, nothing to check by App ID.\n");
	}

	// App ID 0 must fail with a message and log one warning.
	const FSteamResult Invalid = USteamDLCLibrary::InstallSteamDLCByAppId(Context, 0);
	Text += FString::Printf(TEXT("Install App ID 0: %s"), Invalid.IsSuccess() ? TEXT("SUCCEEDED (wrong!)") : *Invalid.Message.ToString());

	Status = FText::FromString(Text);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
