// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Validation/SteamProjectValidator.h"
#include "AssetToolsModule.h"
#include "Assets/SteamAppDefinitionFactory.h"
#include "Cook/SteamCookHelper.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Interfaces/IPluginManager.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Misc/DataValidation.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SandwichSteamEditor.h"
#include "Settings/SteamConfigureAction.h"
#include "Settings/SteamIniWriter.h"

#define LOCTEXT_NAMESPACE "SandwichSteamValidator"

namespace
{
	FSteamValidationCheck MakeCheck(FName Id, ESteamCheckSeverity Severity, const FText& Label, const FText& Detail)
	{
		FSteamValidationCheck Check;
		Check.Id = Id;
		Check.Severity = Severity;
		Check.Label = Label;
		Check.Detail = Detail;
		return Check;
	}

	void CheckSteamPlugin(TArray<FSteamValidationCheck>& Out)
	{
		const FName Id(TEXT("SteamPlugin"));
		const FText Label = LOCTEXT("SteamPluginLabel", "Online Subsystem Steam plugin");

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OnlineSubsystemSteam"));
		if (!Plugin.IsValid())
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("SteamPluginMissing", "The OnlineSubsystemSteam plugin was not found in this engine. Sandwich Steam needs it as the Steam host.")));
		}
		else if (!Plugin->IsEnabled())
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("SteamPluginDisabled", "The OnlineSubsystemSteam plugin is disabled. Enable it in Edit > Plugins and restart the editor.")));
		}
		else
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, LOCTEXT("SteamPluginOk", "Enabled.")));
		}
	}

	void CheckSdk(TArray<FSteamValidationCheck>& Out)
	{
		const FName Id(TEXT("Sdk"));
		const FText Label = LOCTEXT("SdkLabel", "Steamworks SDK");
#if SANDWICHSTEAM_WITH_STEAMWORKS
		Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, LOCTEXT("SdkOk", "Compiled in (engine Steamworks module).")));
#else
		Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("SdkMissing", "SANDWICHSTEAM_WITH_STEAMWORKS is 0 for this platform. Steam features stay inactive.")));
#endif
	}

	void CheckAppId(TArray<FSteamValidationCheck>& Out, const USteamToolSettings* Settings)
	{
		const FName Id(TEXT("AppId"));
		const FText Label = LOCTEXT("AppIdLabel", "Steam App ID");
		const int32 AppId = Settings ? Settings->SteamAppId : 0;

		if (AppId <= 0)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("AppIdMissing", "Not set. Enter your App ID below (480 is Valve's Spacewar test app).")));
		}
		else if (AppId == 480)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Info, Label, LOCTEXT("AppIdSpacewar", "480 is Valve's Spacewar test app. Fine for testing. Replace it with your own App ID before you ship.")));
		}
		else
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, FText::Format(LOCTEXT("AppIdOk", "{0}."), FText::AsNumber(AppId, &FNumberFormattingOptions::DefaultNoGrouping()))));
		}
	}

	void CheckIni(TArray<FSteamValidationCheck>& Out, const USteamToolSettings* Settings)
	{
		const FName Id(TEXT("Ini"));
		const FText Label = LOCTEXT("IniLabel", "DefaultEngine.ini and DefaultGame.ini");
		const int32 AppId = Settings ? Settings->SteamAppId : 0;
		if (AppId <= 0)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Warning, Label, LOCTEXT("IniNoAppId", "Cannot be checked until the Steam App ID is set.")));
			return;
		}

		FString Ini;
		const FString IniPath = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
		if (FPaths::FileExists(IniPath))
		{
			FFileHelper::LoadFileToString(Ini, *IniPath);
		}

		FString GameIni;
		const FString GameIniPath = FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini");
		if (FPaths::FileExists(GameIniPath))
		{
			FFileHelper::LoadFileToString(GameIni, *GameIniPath);
		}

		const bool bVoice = SandwichSteam::Editor::WantsVoiceIni();
		int32 Pending = 0;
		for (const FSteamIniChange& Change : FSteamIniWriter::Diff(Ini, FSteamIniWriter::BuildRequiredEntries(AppId, SandwichSteam::Editor::WantsSessionsIni(), bVoice, Settings->bWriteRelaunchInSteamOff)))
		{
			Pending += Change.Change != ESteamIniChange::Unchanged ? 1 : 0;
		}
		for (const FSteamIniChange& Change : FSteamIniWriter::Diff(GameIni, FSteamIniWriter::BuildGameEntries(bVoice)))
		{
			Pending += Change.Change != ESteamIniChange::Unchanged ? 1 : 0;
		}

		if (Pending == 0)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, LOCTEXT("IniOk", "Platform service, Steam subsystem, SteamDevAppId (and the SteamSockets net driver when Sessions is installed, voice when Voice is installed) are set.")));
			return;
		}

		FSteamValidationCheck Check = MakeCheck(Id, ESteamCheckSeverity::Error, Label,
			FText::Format(LOCTEXT("IniPending", "{0} setting(s) missing or different (DefaultPlatformService=Steam, bEnabled, SteamDevAppId, the SteamSockets net driver when Sessions is installed, bHasVoiceEnabled and bRequiresPushToTalk when Voice is installed). Steam will not start (or voice will not work) until they are set."), FText::AsNumber(Pending)));
		Check.FixLabel = LOCTEXT("IniFix", "Configure Steam...");
		Check.Fix = []() { SandwichSteam::Editor::ConfigureSteam(); };
		Out.Add(MoveTemp(Check));
	}

	void CheckDefinition(TArray<FSteamValidationCheck>& Out, const USteamToolSettings* Settings)
	{
		const FName Id(TEXT("Definition"));
		const FText Label = LOCTEXT("DefinitionLabel", "Steam App Definition");

		const FModuleManager& Modules = FModuleManager::Get();
		const bool bNeeded = Modules.IsModuleLoaded(TEXT("SandwichSteamStats"))
			|| Modules.IsModuleLoaded(TEXT("SandwichSteamAchievements"))
			|| Modules.IsModuleLoaded(TEXT("SandwichSteamLeaderboards"))
			|| Modules.IsModuleLoaded(TEXT("SandwichSteamPresence"))
			|| Modules.IsModuleLoaded(TEXT("SandwichSteamDLC"))
			|| Modules.IsModuleLoaded(TEXT("SandwichSteamSessions"))
			|| Modules.IsModuleLoaded(TEXT("SandwichSteamInput"));

		if (!Settings || Settings->AppDefinition.IsNull())
		{
			if (!bNeeded)
			{
				Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, LOCTEXT("DefinitionNotNeeded", "Not needed: the Stats, Achievements, Leaderboards, Presence, DLC, Sessions and Input modules are not installed.")));
				return;
			}

			FSteamValidationCheck Check = MakeCheck(Id, ESteamCheckSeverity::Warning, Label,
				LOCTEXT("DefinitionMissing", "None assigned. Stats, achievements, leaderboards, rich presence statuses, DLC, session profiles and Steam Input action sets cannot be used by gameplay tag without one."));
			Check.FixLabel = LOCTEXT("DefinitionCreate", "Create and assign");
			Check.Fix = []() { FSteamProjectValidator::CreateAndAssignAppDefinition(); };
			Out.Add(MoveTemp(Check));
			return;
		}

		USteamAppDefinition* Definition = Settings->LoadAppDefinition();
		if (!Definition)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label,
				FText::Format(LOCTEXT("DefinitionBroken", "{0} could not be loaded. Assign an existing asset."), FText::FromString(Settings->AppDefinition.ToSoftObjectPath().ToString()))));
			return;
		}

		FDataValidationContext Context;
		if (Definition->IsDataValid(Context) == EDataValidationResult::Invalid)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label,
				FText::Format(LOCTEXT("DefinitionInvalid", "{0} has errors (duplicate or empty names, progress stats that do not exist, rich presence limits). Open it and run Data Validation for the list."), FText::FromString(Definition->GetName()))));
			return;
		}

		if (!FSteamCookHelper::IsAppDefinitionCooked())
		{
			FSteamValidationCheck Check = MakeCheck(Id, ESteamCheckSeverity::Warning, Label,
				FText::Format(LOCTEXT("DefinitionNotCooked", "{0} is valid but not part of the cook: packaged builds would not find it."), FText::FromString(Definition->GetName())));
			Check.FixLabel = LOCTEXT("DefinitionCookFix", "Add to cook");
			Check.Fix = []() { FSteamCookHelper::EnsureAppDefinitionCooked(); };
			Out.Add(MoveTemp(Check));
			return;
		}

		Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label,
			FText::Format(LOCTEXT("DefinitionOk", "{0}: {1} stats, {2} achievements, {3} leaderboards, {4} presence statuses, {5} DLC, {6} session profiles, {7} input sets. Valid and cooked."),
				FText::FromString(Definition->GetName()), FText::AsNumber(Definition->Stats.Num()), FText::AsNumber(Definition->Achievements.Num()), FText::AsNumber(Definition->Leaderboards.Num()), FText::AsNumber(Definition->Presence.Num()), FText::AsNumber(Definition->DLC.Num()), FText::AsNumber(Definition->Sessions.Num()), FText::AsNumber(Definition->InputSets.Num()))));
	}

	void CheckSessions(TArray<FSteamValidationCheck>& Out)
	{
		if (!SandwichSteam::Editor::WantsSessionsIni())
		{
			return; // The Sessions module is not installed: nothing to check.
		}

		const FName Id(TEXT("SteamSockets"));
		const FText Label = LOCTEXT("SteamSocketsLabel", "SteamSockets plugin (Sessions)");

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SteamSockets"));
		if (!Plugin.IsValid())
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("SteamSocketsMissing", "The SteamSockets plugin was not found in this engine. Sessions carry game traffic over it.")));
		}
		else if (!Plugin->IsEnabled())
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("SteamSocketsDisabled", "The SteamSockets plugin is disabled. Sessions need it: enable it in Edit > Plugins and restart the editor.")));
		}
		else
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, LOCTEXT("SteamSocketsOk", "Enabled. Its net driver entries are checked with DefaultEngine.ini above.")));
		}
	}

	void CheckVoice(TArray<FSteamValidationCheck>& Out)
	{
		if (!SandwichSteam::Editor::WantsVoiceIni())
		{
			return; // The Voice module is not installed: nothing to check.
		}

		const FName Id(TEXT("VoiceInput"));
		const FText Label = LOCTEXT("VoiceInputLabel", "Enhanced Input plugin (Voice)");

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("EnhancedInput"));
		if (!Plugin.IsValid() || !Plugin->IsEnabled())
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("VoiceInputDisabled", "The EnhancedInput plugin is not enabled. Voice binds its push to talk action through it: enable it in Edit > Plugins and restart the editor.")));
			return;
		}

		// The Voice module is optional, so its settings are read from the config instead of through its class.
		FString ActionPath;
		const bool bHasAction = GConfig && GConfig->GetString(TEXT("/Script/SandwichSteamVoice.SteamVoiceSettings"), TEXT("PushToTalkAction"), ActionPath, GGameIni)
			&& !ActionPath.IsEmpty() && ActionPath != TEXT("None");
		if (bHasAction)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, LOCTEXT("VoiceInputOk", "Enabled, and a Push To Talk Action is set in the Voice settings.")));
		}
		else
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Info, Label, LOCTEXT("VoiceInputNoAction", "Enabled. No Push To Talk Action is set in Project Settings > Plugins > Sandwich Steam - Voice: push to talk then works only through Start Talking / Stop Talking calls (open mic needs nothing).")));
		}
	}

	void CheckInput(TArray<FSteamValidationCheck>& Out, const USteamToolSettings* Settings)
	{
		if (!FModuleManager::Get().IsModuleLoaded(TEXT("SandwichSteamInput")))
		{
			return; // The Input module is not installed: nothing to check.
		}

		const FName Id(TEXT("SteamInput"));
		const FText Label = LOCTEXT("SteamInputLabel", "Steam Input (Input module)");

		// The engine's SteamController plugin also initializes Steam Input and reads the same controllers as gamepad keys: input arrives twice.
		const TSharedPtr<IPlugin> EnginePlugin = IPluginManager::Get().FindPlugin(TEXT("SteamController"));
		if (EnginePlugin.IsValid() && EnginePlugin->IsEnabled())
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Error, Label, LOCTEXT("SteamInputEnginePlugin", "The engine's SteamController plugin is enabled. It initializes Steam Input too, so only one of them can work and controllers would deliver input twice. Disable the SteamController plugin in Edit > Plugins and restart the editor.")));
			return;
		}

		const USteamAppDefinition* Definition = Settings ? Settings->LoadAppDefinition() : nullptr;
		if (!Definition || Definition->InputSets.IsEmpty())
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Info, Label, LOCTEXT("SteamInputNoSets", "No Input Sets in the Steam App Definition. Add action sets and actions to get SteamInput_ keys for Enhanced Input; controller, glyph and rumble calls work without them.")));
			return;
		}

		int32 WithoutContext = 0;
		for (const FSteamInputActionSetDef& Set : Definition->InputSets)
		{
			WithoutContext += Set.InputContext.IsNull() ? 1 : 0;
		}

		if (WithoutContext > 0)
		{
			Out.Add(MakeCheck(Id, ESteamCheckSeverity::Warning, Label, FText::Format(LOCTEXT("SteamInputNoContext", "{0} action set(s) have no Enhanced Input mapping context. Their SteamInput_ keys only do something when one of your own contexts maps them."), FText::AsNumber(WithoutContext))));
			return;
		}

		Out.Add(MakeCheck(Id, ESteamCheckSeverity::Ok, Label, LOCTEXT("SteamInputOk", "The SteamController plugin is not enabled and every action set has a mapping context. The action file is generated from the App Definition (Generate Steam Input Actions File).")));
	}

	EMessageSeverity::Type ToMessageSeverity(ESteamCheckSeverity Severity)
	{
		switch (Severity)
		{
		case ESteamCheckSeverity::Error:
			return EMessageSeverity::Error;
		case ESteamCheckSeverity::Warning:
			return EMessageSeverity::Warning;
		default:
			return EMessageSeverity::Info;
		}
	}
}

FName FSteamProjectValidator::GetLogName()
{
	return TEXT("SandwichSteam");
}

TArray<FSteamValidationCheck> FSteamProjectValidator::Run()
{
	const USteamToolSettings* Settings = USteamToolSettings::Get();

	TArray<FSteamValidationCheck> Checks;
	CheckSteamPlugin(Checks);
	CheckSdk(Checks);
	CheckAppId(Checks, Settings);
	CheckIni(Checks, Settings);
	CheckDefinition(Checks, Settings);
	CheckSessions(Checks);
	CheckVoice(Checks);
	CheckInput(Checks, Settings);
	return Checks;
}

void FSteamProjectValidator::RegisterMessageLog()
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	if (!MessageLogModule.IsRegisteredLogListing(GetLogName()))
	{
		MessageLogModule.RegisterLogListing(GetLogName(), LOCTEXT("MessageLogLabel", "Sandwich Steam"));
	}
}

void FSteamProjectValidator::WriteToMessageLog(const TArray<FSteamValidationCheck>& Checks, bool bOpen)
{
	RegisterMessageLog();

	FMessageLog Log(GetLogName());
	Log.NewPage(LOCTEXT("LogPage", "Steam project validation"));

	EMessageSeverity::Type Worst = EMessageSeverity::Info;
	int32 Issues = 0;
	for (const FSteamValidationCheck& Check : Checks)
	{
		if (!Check.IsIssue())
		{
			continue;
		}

		++Issues;
		const EMessageSeverity::Type Severity = ToMessageSeverity(Check.Severity);
		Worst = static_cast<EMessageSeverity::Type>(FMath::Min<int32>(Severity, Worst)); // a lower value is more severe
		Log.Message(Severity, FText::Format(LOCTEXT("LogLine", "{0}: {1}"), Check.Label, Check.Detail));
	}

	if (Issues == 0)
	{
		Log.Info(LOCTEXT("LogAllGood", "Sandwich Steam: every check passed."));
	}
	else if (bOpen)
	{
		Log.Open(Worst, true);
	}
}

void FSteamProjectValidator::RunAndLog(bool bOpen)
{
	WriteToMessageLog(Run(), bOpen);
}

void FSteamProjectValidator::CreateAndAssignAppDefinition()
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	FString PackageName;
	FString AssetName;
	AssetTools.CreateUniqueAssetName(TEXT("/Game/Steam/DA_SteamAppDefinition"), FString(), PackageName, AssetName);

	USteamAppDefinitionFactory* Factory = NewObject<USteamAppDefinitionFactory>();
	UObject* Asset = AssetTools.CreateAsset(AssetName, FPackageName::GetLongPackagePath(PackageName), USteamAppDefinition::StaticClass(), Factory);
	if (!Asset)
	{
		UE_LOG(LogSandwichSteamEditor, Warning, TEXT("Could not create %s."), *PackageName);
		return;
	}

	TArray<UPackage*> Packages;
	Packages.Add(Asset->GetOutermost());
	UEditorLoadingAndSavingUtils::SavePackages(Packages, /*bOnlyDirty*/ false);

	USteamToolSettings* Settings = GetMutableDefault<USteamToolSettings>();
	Settings->AppDefinition = Cast<USteamAppDefinition>(Asset);
	Settings->TryUpdateDefaultConfigFile();
	USteamToolSettings::OnAppDefinitionChanged().Broadcast();

	UE_LOG(LogSandwichSteamEditor, Log, TEXT("Created %s and assigned it as the Steam App Definition."), *PackageName);
}

#undef LOCTEXT_NAMESPACE
