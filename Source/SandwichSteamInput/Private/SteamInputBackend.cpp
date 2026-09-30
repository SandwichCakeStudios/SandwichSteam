// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamInputBackend.h"
#include "Core/SteamBackend.h"
#include "Core/SteamLog.h"
#include "Core/SteamSDK.h"
#include "Engine/GameInstance.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS
namespace
{
	ISteamInput* GetInput()
	{
		return SteamInput();
	}

	InputHandle_t ToHandle(uint64 Handle)
	{
		return static_cast<InputHandle_t>(Handle);
	}

	unsigned short ToMotorSpeed(float Speed)
	{
		return static_cast<unsigned short>(FMath::Clamp(FMath::RoundToInt(Speed * 65535.0f), 0, 65535));
	}

	ESteamInputControllerType MapType(ESteamInputType Type)
	{
		switch (Type)
		{
		case k_ESteamInputType_SteamController:
			return ESteamInputControllerType::SteamController;
		case k_ESteamInputType_SteamDeckController:
			return ESteamInputControllerType::SteamDeck;
		case k_ESteamInputType_XBox360Controller:
			return ESteamInputControllerType::Xbox360;
		case k_ESteamInputType_XBoxOneController:
			return ESteamInputControllerType::XboxOne;
		case k_ESteamInputType_PS3Controller:
			return ESteamInputControllerType::PlayStation3;
		case k_ESteamInputType_PS4Controller:
			return ESteamInputControllerType::PlayStation4;
		case k_ESteamInputType_PS5Controller:
			return ESteamInputControllerType::PlayStation5;
		case k_ESteamInputType_SwitchProController:
			return ESteamInputControllerType::SwitchPro;
		case k_ESteamInputType_SwitchJoyConPair:
		case k_ESteamInputType_SwitchJoyConSingle:
			return ESteamInputControllerType::SwitchJoyCon;
		case k_ESteamInputType_GenericGamepad:
			return ESteamInputControllerType::GenericGamepad;
		case k_ESteamInputType_MobileTouch:
			return ESteamInputControllerType::MobileTouch;
		case k_ESteamInputType_Unknown:
			return ESteamInputControllerType::Unknown;
		default:
			return ESteamInputControllerType::Other;
		}
	}

	ESteamInputGlyphSize ToSdkGlyphSize(ESteamGlyphSize Size)
	{
		switch (Size)
		{
		case ESteamGlyphSize::Small:
			return k_ESteamInputGlyphSize_Small;
		case ESteamGlyphSize::Large:
			return k_ESteamInputGlyphSize_Large;
		default:
			return k_ESteamInputGlyphSize_Medium;
		}
	}
}
#endif

FSteamInputBackend::FSteamInputBackend(UGameInstance* InGameInstance)
	: GameInstance(InGameInstance)
{
}

FSteamInputBackend::~FSteamInputBackend()
{
	Shutdown();
}

bool FSteamInputBackend::Init(const FString& ManifestPath)
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		return true;
	}

	ISteamInput* Input = SandwichSteam::IsSteamClientReady(GameInstance.Get()) ? GetInput() : nullptr;
	if (!Input)
	{
		return false;
	}

	// Explicit frames: the feature calls RunFrame itself, only while it needs to.
	if (!Input->Init(true))
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam Input: ISteamInput::Init failed. Is the engine's SteamController plugin enabled? Only one of them can own Steam Input."));
		return false;
	}
	bInitialized = true;

	if (!ManifestPath.IsEmpty())
	{
		const bool bAccepted = Input->SetInputActionManifestFilePath(TCHAR_TO_UTF8(*ManifestPath));
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: action manifest %s (%s)."), *ManifestPath, bAccepted ? TEXT("accepted") : TEXT("refused"));
	}
	return true;
#else
	return false;
#endif
}

void FSteamInputBackend::Shutdown()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (!bInitialized)
	{
		return;
	}
	bInitialized = false;

	// The Steam client may already be gone when the game instance shuts down.
	if (SandwichSteam::IsSteamClientReady(GameInstance.Get()))
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->Shutdown();
		}
	}
#endif
}

void FSteamInputBackend::RunFrame()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->RunFrame();
		}
	}
#endif
}

void FSteamInputBackend::GetConnectedControllers(TArray<uint64>& OutHandles) const
{
	OutHandles.Reset();
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (!bInitialized)
	{
		return;
	}

	if (ISteamInput* Input = GetInput())
	{
		InputHandle_t Handles[STEAM_INPUT_MAX_COUNT] = {};
		const int32 Count = FMath::Clamp(static_cast<int32>(Input->GetConnectedControllers(Handles)), 0, static_cast<int32>(STEAM_INPUT_MAX_COUNT));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutHandles.Add(static_cast<uint64>(Handles[Index]));
		}
	}
#endif
}

ESteamInputControllerType FSteamInputBackend::GetControllerType(uint64 Handle) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			return MapType(Input->GetInputTypeForHandle(ToHandle(Handle)));
		}
	}
#endif
	return ESteamInputControllerType::Unknown;
}

uint64 FSteamInputBackend::GetActionSetHandle(const FString& SetName) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			return static_cast<uint64>(Input->GetActionSetHandle(TCHAR_TO_UTF8(*SetName)));
		}
	}
#endif
	return 0;
}

uint64 FSteamInputBackend::GetDigitalActionHandle(const FString& ActionName) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			return static_cast<uint64>(Input->GetDigitalActionHandle(TCHAR_TO_UTF8(*ActionName)));
		}
	}
#endif
	return 0;
}

uint64 FSteamInputBackend::GetAnalogActionHandle(const FString& ActionName) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			return static_cast<uint64>(Input->GetAnalogActionHandle(TCHAR_TO_UTF8(*ActionName)));
		}
	}
#endif
	return 0;
}

void FSteamInputBackend::ActivateActionSet(uint64 Controller, uint64 SetHandle) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && SetHandle != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->ActivateActionSet(ToHandle(Controller), static_cast<InputActionSetHandle_t>(SetHandle));
		}
	}
#endif
}

void FSteamInputBackend::ActivateLayer(uint64 Controller, uint64 LayerHandle) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && LayerHandle != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->ActivateActionSetLayer(ToHandle(Controller), static_cast<InputActionSetHandle_t>(LayerHandle));
		}
	}
#endif
}

void FSteamInputBackend::DeactivateLayer(uint64 Controller, uint64 LayerHandle) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && LayerHandle != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->DeactivateActionSetLayer(ToHandle(Controller), static_cast<InputActionSetHandle_t>(LayerHandle));
		}
	}
#endif
}

bool FSteamInputBackend::ReadDigital(uint64 Controller, uint64 Action, bool& bDown) const
{
	bDown = false;
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && Action != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			const InputDigitalActionData_t Data = Input->GetDigitalActionData(ToHandle(Controller), static_cast<InputDigitalActionHandle_t>(Action));
			bDown = Data.bState;
			return Data.bActive;
		}
	}
#endif
	return false;
}

bool FSteamInputBackend::ReadAnalog(uint64 Controller, uint64 Action, float& X, float& Y) const
{
	X = 0.0f;
	Y = 0.0f;
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && Action != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			const InputAnalogActionData_t Data = Input->GetAnalogActionData(ToHandle(Controller), static_cast<InputAnalogActionHandle_t>(Action));
			X = Data.x;
			Y = Data.y;
			return Data.bActive;
		}
	}
#endif
	return false;
}

int32 FSteamInputBackend::GetDigitalOrigin(uint64 Controller, uint64 SetHandle, uint64 Action) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && SetHandle != 0 && Action != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			EInputActionOrigin Origins[STEAM_INPUT_MAX_ORIGINS] = {};
			const int32 Count = Input->GetDigitalActionOrigins(ToHandle(Controller), static_cast<InputActionSetHandle_t>(SetHandle), static_cast<InputDigitalActionHandle_t>(Action), Origins);
			if (Count > 0 && Origins[0] != k_EInputActionOrigin_None)
			{
				return static_cast<int32>(Origins[0]);
			}
		}
	}
#endif
	return 0;
}

int32 FSteamInputBackend::GetAnalogOrigin(uint64 Controller, uint64 SetHandle, uint64 Action) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && SetHandle != 0 && Action != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			EInputActionOrigin Origins[STEAM_INPUT_MAX_ORIGINS] = {};
			const int32 Count = Input->GetAnalogActionOrigins(ToHandle(Controller), static_cast<InputActionSetHandle_t>(SetHandle), static_cast<InputAnalogActionHandle_t>(Action), Origins);
			if (Count > 0 && Origins[0] != k_EInputActionOrigin_None)
			{
				return static_cast<int32>(Origins[0]);
			}
		}
	}
#endif
	return 0;
}

FString FSteamInputBackend::GetGlyphPath(int32 Origin, ESteamGlyphSize Size) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && Origin != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			const char* Path = Input->GetGlyphPNGForActionOrigin(static_cast<EInputActionOrigin>(Origin), ToSdkGlyphSize(Size), 0);
			return Path ? FString(UTF8_TO_TCHAR(Path)) : FString();
		}
	}
#endif
	return FString();
}

FString FSteamInputBackend::GetOriginName(int32 Origin) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized && Origin != 0)
	{
		if (ISteamInput* Input = GetInput())
		{
			const char* Name = Input->GetStringForActionOrigin(static_cast<EInputActionOrigin>(Origin));
			return Name ? FString(UTF8_TO_TCHAR(Name)) : FString();
		}
	}
#endif
	return FString();
}

void FSteamInputBackend::SetVibration(uint64 Controller, float Left, float Right, float LeftTrigger, float RightTrigger) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->TriggerVibrationExtended(ToHandle(Controller), ToMotorSpeed(Left), ToMotorSpeed(Right), ToMotorSpeed(LeftTrigger), ToMotorSpeed(RightTrigger));
		}
	}
#endif
}

void FSteamInputBackend::SetLedColor(uint64 Controller, const FColor& Color) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->SetLEDColor(ToHandle(Controller), Color.R, Color.G, Color.B, k_ESteamInputLEDFlag_SetColor);
		}
	}
#endif
}

void FSteamInputBackend::ResetLedColor(uint64 Controller) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			Input->SetLEDColor(ToHandle(Controller), 0, 0, 0, k_ESteamInputLEDFlag_RestoreUserDefault);
		}
	}
#endif
}

bool FSteamInputBackend::ShowBindingPanel(uint64 Controller) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (bInitialized)
	{
		if (ISteamInput* Input = GetInput())
		{
			return Input->ShowBindingPanel(ToHandle(Controller));
		}
	}
#endif
	return false;
}
