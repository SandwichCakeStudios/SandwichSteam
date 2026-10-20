// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "GameFramework/Info.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "SteamVoiceTypes.h"
#include "SteamVoiceAuthority.generated.h"

class ASteamVoiceAuthority;
struct FSteamVoicePlayerRulesArray;

/** One player's server rules (only players with a restriction are listed). */
USTRUCT()
struct FSteamVoicePlayerRulesItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FSteamId Player;

	UPROPERTY()
	FSteamVoicePlayerRules Rules;

	void PostReplicatedAdd(const FSteamVoicePlayerRulesArray& Array);
	void PostReplicatedChange(const FSteamVoicePlayerRulesArray& Array);
	void PreReplicatedRemove(const FSteamVoicePlayerRulesArray& Array);
};

/** The player rules of the authority, replicated as a delta (only changed players travel). */
USTRUCT()
struct FSteamVoicePlayerRulesArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSteamVoicePlayerRulesItem> Items;

	/** The actor that holds this array (told about replicated changes). */
	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<ASteamVoiceAuthority> Owner = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FSteamVoicePlayerRulesItem, FSteamVoicePlayerRulesArray>(Items, DeltaParams, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FSteamVoicePlayerRulesArray> : public TStructOpsTypeTraitsBase2<FSteamVoicePlayerRulesArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

/**
 * The server's voice rules for the current world (Server Authority in the Voice settings). The Steam Voice subsystem spawns it on the
 * server (listen host or standalone) when a world starts and fills it; it replicates to every client, whose subsystem applies it.
 * Holds the game rules (Voice Allowed, Pawnless Voice) and per player rules keyed by Steam ID, plus a version the server raises on
 * every change and on every new authority (after travel). Not placeable: use the Server Set functions of the subsystem.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class SANDWICHSTEAMVOICE_API ASteamVoiceAuthority : public AInfo
{
	GENERATED_BODY()

public:
	ASteamVoiceAuthority();

	//~ Begin AActor
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor

	/** 0 until the server filled the rules; rises with every change. */
	int32 GetRulesVersion() const { return RulesVersion; }
	bool IsVoiceAllowed() const { return bVoiceAllowed; }
	ESteamVoicePawnlessMode GetPawnlessVoice() const { return PawnlessVoice; }
	const TArray<FSteamVoicePlayerRulesItem>& GetPlayerRules() const { return PlayerRules.Items; }

	/** Server: the game rules. */
	void SetGameRules(bool bAllowed, ESteamVoicePawnlessMode Pawnless);

	/** Server: one player's rules. Default rules remove the player from the list. */
	void SetPlayerRules(FSteamId Player, const FSteamVoicePlayerRules& Rules);

	/** Server: replaces every player's rules (a new authority after travel). Only players with a restriction are kept. */
	void ResetPlayerRules(const TMap<int64, FSteamVoicePlayerRules>& Rules);

	/** Server: the version clients compare. Set after the rules of one change. */
	void SetRulesVersion(int32 Version);

	/** The player rules array received a change (called by the array items on clients). */
	void NotifyRulesReplicated();

private:
	UFUNCTION()
	void OnRep_Rules();

	UPROPERTY(Replicated)
	FSteamVoicePlayerRulesArray PlayerRules;

	UPROPERTY(ReplicatedUsing = OnRep_Rules)
	int32 RulesVersion = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Rules)
	bool bVoiceAllowed = true;

	UPROPERTY(ReplicatedUsing = OnRep_Rules)
	ESteamVoicePawnlessMode PawnlessVoice = ESteamVoicePawnlessMode::Play2D;
};
