// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamToolSettings.h"
#include "Core/SteamLog.h"
#include "Data/SteamAppDefinition.h"
#include "Misc/Paths.h"

const USteamToolSettings* USteamToolSettings::Get()
{
	return GetDefault<USteamToolSettings>();
}

FString USteamToolSettings::GetDataDirectory() const
{
	FString Directory = DataDirectory.Path.TrimStartAndEnd();
	if (Directory.IsEmpty())
	{
		Directory = FPaths::ProjectSavedDir() / TEXT("SandwichSteam");
	}
	else if (FPaths::IsRelative(Directory))
	{
		Directory = FPaths::ProjectDir() / Directory;
	}
	Directory = FPaths::ConvertRelativePathToFull(Directory);
	FPaths::NormalizeDirectoryName(Directory);
	return Directory;
}

bool USteamToolSettings::IsFeatureEnabled(const FGameplayTag& FeatureTag) const
{
	return !FeatureTag.IsValid() || !DisabledFeatures.HasTagExact(FeatureTag);
}

void USteamToolSettings::ApplyLogVerbosity() const
{
#if !NO_LOGGING
	LogSandwichSteam.SetVerbosity(bVerboseLogging ? ELogVerbosity::Verbose : ELogVerbosity::Log);
#endif
}

USteamAppDefinition* USteamToolSettings::LoadAppDefinition() const
{
	return AppDefinition.IsNull() ? nullptr : AppDefinition.LoadSynchronous();
}

FName USteamToolSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName USteamToolSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
USteamToolSettings::FOnAppDefinitionChanged& USteamToolSettings::OnAppDefinitionChanged()
{
	static FOnAppDefinitionChanged Delegate;
	return Delegate;
}

void USteamToolSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(USteamToolSettings, bVerboseLogging))
	{
		ApplyLogVerbosity();
	}
	else if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(USteamToolSettings, AppDefinition))
	{
		OnAppDefinitionChanged().Broadcast();
	}
}
#endif
