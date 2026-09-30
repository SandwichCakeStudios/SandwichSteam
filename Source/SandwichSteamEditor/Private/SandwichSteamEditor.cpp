// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamEditor.h"
#include "Assets/SteamAppDefinitionCustomization.h"
#include "Assets/SteamDashboardAppDefinitionPage.h"
#include "Cook/SteamCookHelper.h"
#include "Core/SteamToolSettings.h"
#include "Dashboard/SSteamDashboardPanel.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Data/SteamAppDefinition.h"
#include "PropertyEditorModule.h"
#include "Publish/SSteamCmdSetupPage.h"
#include "Publish/SSteamPublishPanel.h"
#include "Publish/SteamPublishSettings.h"
#include "Publish/SteamPublishSettingsCustomization.h"
#include "Sdk/SteamDashboardSdkPage.h"
#include "Settings/SteamToolSettingsCustomization.h"
#include "Style/SteamToolStyle.h"
#include "ToolMenus.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Validation/SteamProjectValidator.h"

DEFINE_LOG_CATEGORY(LogSandwichSteamEditor);

#define LOCTEXT_NAMESPACE "SandwichSteamEditor"

FSandwichSteamEditorModule::FSandwichSteamEditorModule() = default;
FSandwichSteamEditorModule::~FSandwichSteamEditorModule() = default;

void FSandwichSteamEditorModule::StartupModule()
{
	FSteamToolStyle::Register();
	FSteamProjectValidator::RegisterMessageLog();

	CookHelper = MakeUnique<FSteamCookHelper>();
	CookHelper->Register();

	RegisterDetailCustomizations();
	SandwichSteam::Editor::RegisterPublishTab();
	SandwichSteam::Editor::RegisterDashboardTab();
	SandwichSteam::Editor::RegisterSteamworksSdkDashboardPage();
	SandwichSteam::Editor::RegisterAppDefinitionDashboardPage();
	SandwichSteam::Editor::RegisterSteamCmdDashboardPage();
	SandwichSteam::Editor::RegisterPublishDashboardPage();

	ObjectChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FSandwichSteamEditorModule::HandleObjectPropertyChanged);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSandwichSteamEditorModule::RegisterMenus));
}

void FSandwichSteamEditorModule::ShutdownModule()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectChangedHandle);
	ObjectChangedHandle.Reset();

	if (CookHelper.IsValid())
	{
		CookHelper->Unregister();
		CookHelper.Reset();
	}

	SandwichSteam::Editor::UnregisterDashboardPage(TEXT("Publish"));
	SandwichSteam::Editor::UnregisterDashboardPage(TEXT("SteamCmd"));
	SandwichSteam::Editor::UnregisterDashboardPage(TEXT("AppDefinition"));
	SandwichSteam::Editor::UnregisterDashboardPage(TEXT("SteamworksSdk"));
	SandwichSteam::Editor::UnregisterDashboardTab();
	SandwichSteam::Editor::UnregisterPublishTab();

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	UnregisterDetailCustomizations();
	FSteamToolStyle::Unregister();
}

void FSandwichSteamEditorModule::RegisterDetailCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(USteamToolSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FSteamToolSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USteamAppDefinition::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FSteamAppDefinitionCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USteamPublishSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FSteamPublishSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(USteamPublishUserSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FSteamPublishUserSettingsCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FSandwichSteamEditorModule::UnregisterDetailCustomizations()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomClassLayout(USteamToolSettings::StaticClass()->GetFName());
		PropertyModule->UnregisterCustomClassLayout(USteamAppDefinition::StaticClass()->GetFName());
		PropertyModule->UnregisterCustomClassLayout(USteamPublishSettings::StaticClass()->GetFName());
		PropertyModule->UnregisterCustomClassLayout(USteamPublishUserSettings::StaticClass()->GetFName());
		PropertyModule->NotifyCustomizationModuleChanged();
	}
}

void FSandwichSteamEditorModule::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	// Committed edits only (not every step of a slider drag). The settings strip refreshes itself; this keeps the Message Log page current.
	if (Event.ChangeType != EPropertyChangeType::Interactive && Object == GetDefault<USteamToolSettings>())
	{
		FSteamProjectValidator::RunAndLog(/*bOpen*/ false);
	}
}

void FSandwichSteamEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection("SandwichSteam", LOCTEXT("SectionLabel", "Sandwich Steam"));

	const FSlateIcon ToolIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Icon16");

	Section.AddMenuEntry(
		"SandwichSteamDashboard",
		LOCTEXT("DashboardLabel", "Open Steam Dashboard"),
		LOCTEXT("DashboardTooltip", "One place for Steam setup, the App Definition and publishing."),
		ToolIcon,
		FUIAction(FExecuteAction::CreateStatic(&SandwichSteam::Editor::SandwichSteamDashboard)));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSandwichSteamEditorModule, SandwichSteamEditor)
