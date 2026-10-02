// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamCmdDownloadJob.h"
#include "Core/SteamToolSettings.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Publish/SteamProcessRunner.h"
#include "Publish/SteamPublishSettings.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "SandwichSteamCmdDownloadJob"

namespace SandwichSteam::Editor
{
	FString GetSteamCmdRoot()
	{
		return USteamToolSettings::Get()->GetDataDirectory() / TEXT("SteamCMD");
	}

	FString GetHostPlatformFolderName()
	{
#if PLATFORM_MAC
		return TEXT("Mac");
#elif PLATFORM_LINUX
		return TEXT("Linux");
#else
		return TEXT("Win64");
#endif
	}

	FString GetSteamCmdInstallDir()
	{
		return GetSteamCmdRoot() / GetHostPlatformFolderName();
	}

	FString GetSteamCmdExecutableName()
	{
#if PLATFORM_WINDOWS
		return TEXT("steamcmd.exe");
#else
		return TEXT("steamcmd.sh");
#endif
	}

	FString ManifestToJson(const FSteamCmdManifest& Manifest)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("platform"), Manifest.Platform);
		Root->SetStringField(TEXT("url"), Manifest.Url);
		Root->SetStringField(TEXT("downloadedUtc"), Manifest.DownloadedUtc.ToIso8601());
		Root->SetStringField(TEXT("installedPath"), Manifest.InstalledPath);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		return Json;
	}

	bool ManifestFromJson(const FString& Json, FSteamCmdManifest& OutManifest)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			return false;
		}

		FString Downloaded;
		Root->TryGetStringField(TEXT("platform"), OutManifest.Platform);
		Root->TryGetStringField(TEXT("url"), OutManifest.Url);
		Root->TryGetStringField(TEXT("downloadedUtc"), Downloaded);
		FDateTime::ParseIso8601(*Downloaded, OutManifest.DownloadedUtc);
		Root->TryGetStringField(TEXT("installedPath"), OutManifest.InstalledPath);
		return true;
	}

	TOptional<FSteamCmdManifest> ReadSteamCmdManifest(const FString& InstallRoot)
	{
		FString Json;
		FSteamCmdManifest Manifest;
		if (FFileHelper::LoadFileToString(Json, *(InstallRoot / TEXT("Manifest.json"))) && ManifestFromJson(Json, Manifest))
		{
			return Manifest;
		}
		return TOptional<FSteamCmdManifest>();
	}

	bool WriteSteamCmdManifest(const FString& InstallRoot, const FSteamCmdManifest& Manifest)
	{
		IFileManager::Get().MakeDirectory(*InstallRoot, true);
		return FFileHelper::SaveStringToFile(ManifestToJson(Manifest), *(InstallRoot / TEXT("Manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool PickShallowestPath(const TArray<FString>& Candidates, FString& OutPath)
	{
		if (Candidates.IsEmpty())
		{
			return false;
		}

		OutPath = Candidates[0];
		int32 BestDepth = TNumericLimits<int32>::Max();
		for (const FString& Candidate : Candidates)
		{
			FString Normalized = Candidate;
			FPaths::NormalizeFilename(Normalized);
			int32 Depth = 0;
			for (const TCHAR Char : Normalized)
			{
				Depth += Char == TEXT('/') ? 1 : 0;
			}
			if (Depth < BestDepth)
			{
				BestDepth = Depth;
				OutPath = Candidate;
			}
		}
		return true;
	}

	FString FindSteamCmdExecutable(const FString& InstallDir)
	{
		TArray<FString> Matches;
		IFileManager::Get().FindFilesRecursive(Matches, *InstallDir, *GetSteamCmdExecutableName(), /*Files*/ true, /*Directories*/ false);

		FString Best;
		return PickShallowestPath(Matches, Best) ? Best : FString();
	}
}

FSteamCmdDownloadJob::FSteamCmdDownloadJob() = default;

FSteamCmdDownloadJob::~FSteamCmdDownloadJob()
{
	if (DownloadRequest.IsValid())
	{
		DownloadRequest->OnRequestProgress64().Unbind();
		DownloadRequest->OnProcessRequestComplete().Unbind();
		DownloadRequest->CancelRequest();
	}
	// ProcessRunner kills its process when destroyed.
}

bool FSteamCmdDownloadJob::Start(FString& OutError)
{
	check(IsInGameThread());
	if (bRunning)
	{
		OutError = LOCTEXT("Busy", "A SteamCMD download is already running.").ToString();
		return false;
	}

	DownloadUrl = USteamPublishSettings::Get()->GetSteamCmdDownloadUrl();
	if (DownloadUrl.IsEmpty())
	{
		OutError = LOCTEXT("NoUrl", "No SteamCMD download URL is set for this platform (see the SteamCMD Download category of Sandwich Steam - Publish).").ToString();
		return false;
	}

	bRunning = true;
	bCancelRequested = false;
	Log(FString::Printf(TEXT("SteamCMD download started (%s)"), *DownloadUrl));
	BeginDownload();
	return true;
}

void FSteamCmdDownloadJob::Cancel()
{
	check(IsInGameThread());
	if (!bRunning)
	{
		return;
	}
	bCancelRequested = true;
	Log(TEXT("Cancel requested"), ESteamLogSeverity::Warning);

	bool bStopped = false;
	if (DownloadRequest.IsValid() && DownloadRequest->GetStatus() == EHttpRequestStatus::Processing)
	{
		DownloadRequest->CancelRequest();
		bStopped = true;
	}
	if (ProcessRunner.IsValid() && ProcessRunner->IsRunning())
	{
		ProcessRunner->Cancel();
		bStopped = true;
	}
	// A running request/process reports back through its own completion delegate; otherwise stop here.
	if (!bStopped)
	{
		Finish(false, LOCTEXT("Canceled", "Cancelled."));
	}
}

void FSteamCmdDownloadJob::Log(const FString& Line, ESteamLogSeverity Severity)
{
	LogDelegate.Broadcast(Line, Severity);
}

void FSteamCmdDownloadJob::Finish(bool bSuccess, const FText& Message, const FString& ExePath)
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;

	const FText FinalMessage = bCancelRequested && !bSuccess ? LOCTEXT("Canceled", "Cancelled.") : Message;
	Log(FinalMessage.ToString(), bSuccess ? ESteamLogSeverity::Info : (bCancelRequested ? ESteamLogSeverity::Warning : ESteamLogSeverity::Error));

	ProgressDelegate.Broadcast(bSuccess ? 1.f : 0.f, FinalMessage);
	FinishedDelegate.Broadcast(bSuccess, FinalMessage, bSuccess ? ExePath : FString());
}

void FSteamCmdDownloadJob::BeginDownload()
{
	ProgressDelegate.Broadcast(0.f, LOCTEXT("StepDownload", "Downloading SteamCMD..."));

	const FString DownloadDir = SandwichSteam::Editor::GetSteamCmdRoot() / TEXT("Download");
	IFileManager::Get().MakeDirectory(*DownloadDir, true);
	DownloadedArchivePath = DownloadDir / FPaths::GetCleanFilename(DownloadUrl);
	Log(FString::Printf(TEXT("> GET %s"), *DownloadUrl));

	DownloadRequest = FHttpModule::Get().CreateRequest();
	DownloadRequest->SetURL(DownloadUrl);
	DownloadRequest->SetVerb(TEXT("GET"));

	const TWeakPtr<FSteamCmdDownloadJob> WeakSelf = AsShared();
	// [verify] UE 5.8 http progress delegate: OnRequestProgress64(FHttpRequestPtr, uint64 BytesSent, uint64 BytesReceived).
	// If this signature differs, fix it here; a wrong signature only loses the progress bar's fill during download, nothing else.
	DownloadRequest->OnRequestProgress64().BindLambda([WeakSelf](FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
	{
		if (const TSharedPtr<FSteamCmdDownloadJob> Job = WeakSelf.Pin())
		{
			Job->HandleDownloadProgress(Request, BytesSent, BytesReceived);
		}
	});
	DownloadRequest->OnProcessRequestComplete().BindLambda([WeakSelf](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
	{
		if (const TSharedPtr<FSteamCmdDownloadJob> Job = WeakSelf.Pin())
		{
			Job->HandleDownloadComplete(Request, Response, bConnectedSuccessfully);
		}
	});

	if (!DownloadRequest->ProcessRequest())
	{
		Finish(false, LOCTEXT("DownloadStartFailed", "Could not start the SteamCMD download."));
	}
}

void FSteamCmdDownloadJob::HandleDownloadProgress(FHttpRequestPtr Request, uint64 /*BytesSent*/, uint64 BytesReceived)
{
	if (!bRunning)
	{
		return;
	}

	float Fraction = -1.f;
	const FHttpResponsePtr Response = Request.IsValid() ? Request->GetResponse() : nullptr;
	const int32 ContentLength = Response.IsValid() ? Response->GetContentLength() : 0;
	if (ContentLength > 0)
	{
		Fraction = FMath::Clamp(static_cast<float>(BytesReceived) / static_cast<float>(ContentLength), 0.f, 1.f);
	}
	ProgressDelegate.Broadcast(Fraction >= 0.f ? Fraction * 0.6f : -1.f, LOCTEXT("StepDownload", "Downloading SteamCMD..."));
}

void FSteamCmdDownloadJob::HandleDownloadComplete(FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bRunning)
	{
		return;
	}
	if (bCancelRequested)
	{
		Finish(false, FText::GetEmpty());
		return;
	}
	if (!bConnectedSuccessfully || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
		Finish(false, FText::Format(LOCTEXT("DownloadFailed", "Download failed (HTTP {0})."), Code));
		return;
	}
	if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *DownloadedArchivePath))
	{
		Finish(false, FText::Format(LOCTEXT("SaveFailed", "Could not write {0}."), FText::FromString(DownloadedArchivePath)));
		return;
	}

	Log(FString::Printf(TEXT("Downloaded %s"), *DownloadedArchivePath));
	BeginExtract();
}

void FSteamCmdDownloadJob::BeginExtract()
{
	ProgressDelegate.Broadcast(0.6f, LOCTEXT("StepExtract", "Extracting SteamCMD..."));

	InstallDir = SandwichSteam::Editor::GetSteamCmdInstallDir();
	IFileManager::Get().MakeDirectory(*InstallDir, true);

	ProcessRunner = MakeShared<FSteamProcessRunner>();
	ProcessRunner->OnLine().AddSP(this, &FSteamCmdDownloadJob::HandleProcessLine);
	ProcessRunner->OnFinished().AddSP(this, &FSteamCmdDownloadJob::HandleExtractFinished);

	// Windows 10 (1803+) ships tar.exe (bsdtar), which extracts .zip as well as .tar.gz; Linux and macOS always have tar.
	// This avoids a bundled zip/gzip library for one archive per platform.
#if PLATFORM_WINDOWS
	const FString Executable = TEXT("tar.exe");
	const FString Arguments = FString::Printf(TEXT("-xf \"%s\" -C \"%s\""), *DownloadedArchivePath, *InstallDir);
#else
	const FString Executable = TEXT("tar");
	const FString Arguments = FString::Printf(TEXT("-xzf \"%s\" -C \"%s\""), *DownloadedArchivePath, *InstallDir);
#endif
	Log(FString::Printf(TEXT("> %s %s"), *Executable, *Arguments));

	FString Error;
	if (!ProcessRunner->Start(Executable, Arguments, Error))
	{
		Finish(false, FText::Format(LOCTEXT("ExtractStartFailed", "{0} Extract {1} manually; see See SteamCMD offical documentation."), FText::FromString(Error), FText::FromString(DownloadedArchivePath)));
	}
}

void FSteamCmdDownloadJob::HandleProcessLine(const FString& Line)
{
	Log(Line);
}

void FSteamCmdDownloadJob::HandleExtractFinished(int32 ReturnCode, bool bCanceled)
{
	if (!bRunning)
	{
		return;
	}
	if (bCanceled || bCancelRequested)
	{
		Finish(false, FText::GetEmpty());
		return;
	}
	if (ReturnCode != 0)
	{
		Finish(false, FText::Format(LOCTEXT("ExtractFailed", "Extract failed (exit code {0}). Extract {1} manually; see See SteamCMD offical documentation."), ReturnCode, FText::FromString(DownloadedArchivePath)));
		return;
	}

	FoundExePath = SandwichSteam::Editor::FindSteamCmdExecutable(InstallDir);
	if (FoundExePath.IsEmpty())
	{
		Finish(false, FText::Format(LOCTEXT("ExeNotFound", "SteamCMD was extracted but {0} was not found under {1}. See See SteamCMD offical documentation."),
			FText::FromString(SandwichSteam::Editor::GetSteamCmdExecutableName()), FText::FromString(InstallDir)));
		return;
	}

#if !PLATFORM_WINDOWS
	// The tarball ships steamcmd.sh executable already; this is cheap insurance against a generic tar invocation
	// losing that bit, not something the extract depends on succeeding.
	FPlatformProcess::ExecProcess(TEXT("/bin/chmod"), *FString::Printf(TEXT("+x \"%s\""), *FoundExePath), nullptr, nullptr, nullptr);
#endif

	Log(FString::Printf(TEXT("Found %s"), *FoundExePath));
	BeginSelfUpdate();
}

void FSteamCmdDownloadJob::BeginSelfUpdate()
{
	ProgressDelegate.Broadcast(0.85f, LOCTEXT("StepSelfUpdate", "Updating SteamCMD..."));

	ProcessRunner = MakeShared<FSteamProcessRunner>();
	ProcessRunner->OnLine().AddSP(this, &FSteamCmdDownloadJob::HandleProcessLine);
	ProcessRunner->OnFinished().AddSP(this, &FSteamCmdDownloadJob::HandleSelfUpdateFinished);

	// A bare +quit never logs in, so the login/Steam Guard state machine of FSteamCmdRunner is not needed here.
	const FString Arguments = TEXT("+@ShutdownOnFailedCommand 1 +@NoPromptForPassword 1 +quit");
	Log(FString::Printf(TEXT("> %s %s"), *FoundExePath, *Arguments));

	FString Error;
	if (!ProcessRunner->Start(FoundExePath, Arguments, Error))
	{
		// Not fatal: the exe exists and can be used, Publish's own SteamCMD run will update it anyway.
		Log(FString::Printf(TEXT("Self-update could not start: %s"), *Error), ESteamLogSeverity::Warning);
		WriteManifestAndFinish();
	}
}

void FSteamCmdDownloadJob::HandleSelfUpdateFinished(int32 ReturnCode, bool bCanceled)
{
	if (!bRunning)
	{
		return;
	}
	if (bCanceled || bCancelRequested)
	{
		Finish(false, FText::GetEmpty());
		return;
	}
	if (ReturnCode != 0)
	{
		Log(FString::Printf(TEXT("SteamCMD self-update returned exit code %d; continuing anyway."), ReturnCode), ESteamLogSeverity::Warning);
	}
	WriteManifestAndFinish();
}

void FSteamCmdDownloadJob::WriteManifestAndFinish()
{
	ProgressDelegate.Broadcast(0.97f, LOCTEXT("StepManifest", "Finishing up..."));

	FSteamCmdManifest Manifest;
	Manifest.Platform = SandwichSteam::Editor::GetHostPlatformFolderName();
	Manifest.Url = DownloadUrl;
	Manifest.DownloadedUtc = FDateTime::UtcNow();
	Manifest.InstalledPath = FoundExePath;
	if (!SandwichSteam::Editor::WriteSteamCmdManifest(SandwichSteam::Editor::GetSteamCmdRoot(), Manifest))
	{
		Log(TEXT("Could not write Manifest.json (not fatal)."), ESteamLogSeverity::Warning);
	}

	Finish(true, FText::Format(LOCTEXT("InstalledMessage", "SteamCMD installed at {0}."), FText::FromString(FoundExePath)), FoundExePath);
}

#undef LOCTEXT_NAMESPACE
