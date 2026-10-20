// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceAuthority.h"
#include "Core/SteamEngineCompat.h"
#include "Net/UnrealNetwork.h"
#include "SteamVoiceSubsystem.h"

namespace
{
	/** Rules change rarely; a change still reaches clients within a tenth of a second. */
	constexpr float AuthorityNetUpdateFrequency = 10.0f;
}

void FSteamVoicePlayerRulesItem::PostReplicatedAdd(const FSteamVoicePlayerRulesArray& Array)
{
	if (Array.Owner)
	{
		Array.Owner->NotifyRulesReplicated();
	}
}

void FSteamVoicePlayerRulesItem::PostReplicatedChange(const FSteamVoicePlayerRulesArray& Array)
{
	if (Array.Owner)
	{
		Array.Owner->NotifyRulesReplicated();
	}
}

void FSteamVoicePlayerRulesItem::PreReplicatedRemove(const FSteamVoicePlayerRulesArray& Array)
{
	if (Array.Owner)
	{
		Array.Owner->NotifyRulesReplicated();
	}
}

ASteamVoiceAuthority::ASteamVoiceAuthority()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SandwichSteam::Compat::SetNetUpdateFrequency(*this, AuthorityNetUpdateFrequency);
	PlayerRules.Owner = this;
}

void ASteamVoiceAuthority::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASteamVoiceAuthority, PlayerRules);
	DOREPLIFETIME(ASteamVoiceAuthority, RulesVersion);
	DOREPLIFETIME(ASteamVoiceAuthority, bVoiceAllowed);
	DOREPLIFETIME(ASteamVoiceAuthority, PawnlessVoice);
}

void ASteamVoiceAuthority::BeginPlay()
{
	Super::BeginPlay();

	// Clients learn about the rules here (the first replication arrives with the actor); the server already knows its own actor.
	if (USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(this))
	{
		Voice->HandleAuthorityChanged(this, false);
	}
}

void ASteamVoiceAuthority::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(this))
	{
		Voice->HandleAuthorityChanged(this, true);
	}

	Super::EndPlay(EndPlayReason);
}

void ASteamVoiceAuthority::SetGameRules(bool bAllowed, ESteamVoicePawnlessMode Pawnless)
{
	bVoiceAllowed = bAllowed;
	PawnlessVoice = Pawnless;
}

void ASteamVoiceAuthority::SetPlayerRules(FSteamId Player, const FSteamVoicePlayerRules& Rules)
{
	const int32 Index = PlayerRules.Items.IndexOfByPredicate([Player](const FSteamVoicePlayerRulesItem& Item) { return Item.Player == Player; });
	if (Rules.IsDefault())
	{
		if (Index != INDEX_NONE)
		{
			PlayerRules.Items.RemoveAtSwap(Index);
			PlayerRules.MarkArrayDirty();
		}
		return;
	}

	FSteamVoicePlayerRulesItem& Item = Index != INDEX_NONE ? PlayerRules.Items[Index] : PlayerRules.Items.AddDefaulted_GetRef();
	Item.Player = Player;
	Item.Rules = Rules;
	PlayerRules.MarkItemDirty(Item);
}

void ASteamVoiceAuthority::ResetPlayerRules(const TMap<int64, FSteamVoicePlayerRules>& Rules)
{
	PlayerRules.Items.Reset(Rules.Num());
	for (const TPair<int64, FSteamVoicePlayerRules>& Pair : Rules)
	{
		if (!Pair.Value.IsDefault())
		{
			FSteamVoicePlayerRulesItem& Item = PlayerRules.Items.AddDefaulted_GetRef();
			Item.Player = FSteamId(Pair.Key);
			Item.Rules = Pair.Value;
			PlayerRules.MarkItemDirty(Item);
		}
	}
	PlayerRules.MarkArrayDirty();
}

void ASteamVoiceAuthority::SetRulesVersion(int32 Version)
{
	RulesVersion = Version;
}

void ASteamVoiceAuthority::NotifyRulesReplicated()
{
	// Every replicated property of one update is applied before BeginPlay on a new actor: the subsystem reads them there.
	if (HasActorBegunPlay())
	{
		if (USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(this))
		{
			Voice->HandleAuthorityChanged(this, false);
		}
	}
}

void ASteamVoiceAuthority::OnRep_Rules()
{
	NotifyRulesReplicated();
}
