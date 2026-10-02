// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamSDK.h"

#include <atomic>

#if SANDWICHSTEAM_WITH_STEAMWORKS

/**
 * One pending Steam API call result (SteamAPICall_t) whose completion is delivered on the game thread.
 *
 * The raw CCallResult handler runs on Steam's callback thread. It only copies the payload and enqueues
 * work on the dispatcher. The user handler then runs on the game thread, after which the item removes
 * itself from the owner's pending list.
 *
 * Ownership: the owning subsystem keeps a FPendingList and only touches it on the game thread. Clearing
 * the list (ShutdownFeature) cancels every request; work already queued for a destroyed item is skipped.
 *
 * Usage:
 *   TSteamCallResult<LeaderboardFindResult_t>::Start(PendingFind, Dispatcher, SteamUserStats()->FindLeaderboard(Name),
 *       [WeakSelf](const LeaderboardFindResult_t& Result, bool bIOFailure) { ... });
 */
template <typename TPayload>
class TSteamCallResult
{
public:
	using FHandler = TFunction<void(const TPayload& Payload, bool bIOFailure)>;
	using FPendingList = TArray<TUniquePtr<TSteamCallResult>>;
	using FDispatcherRef = TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>;

	/** Game thread only. Returns nullptr when Call is invalid. The returned pointer is owned by Pending. */
	static TSteamCallResult* Start(FPendingList& Pending, const FDispatcherRef& Dispatcher, SteamAPICall_t Call, FHandler Handler)
	{
		check(IsInGameThread());

		if (Call == k_uAPICallInvalid)
		{
			return nullptr;
		}

		TUniquePtr<TSteamCallResult> Item = MakeUnique<TSteamCallResult>(&Pending, Dispatcher, MoveTemp(Handler));
		TSteamCallResult* RawItem = Item.Get();
		Pending.Add(MoveTemp(Item));

		// Registered after the item is in the list: even if Steam completes instantly on another thread,
		// the completion work only runs later on the game thread.
		RawItem->CallResult.Set(Call, RawItem, &TSteamCallResult::OnRawResult);
		return RawItem;
	}

	TSteamCallResult(FPendingList* InPending, const FDispatcherRef& InDispatcher, FHandler InHandler)
		: State(MakeShared<FState, ESPMode::ThreadSafe>(InPending, MoveTemp(InHandler)))
		, Dispatcher(InDispatcher)
	{
	}

	TSteamCallResult(const TSteamCallResult&) = delete;
	TSteamCallResult& operator=(const TSteamCallResult&) = delete;

	~TSteamCallResult()
	{
		// Work that is already queued sees the flag and does nothing. CallResult unregisters in its destructor.
		State->bCancelled.store(true, std::memory_order_release);
	}

private:
	struct FState
	{
		FState(FPendingList* InPending, FHandler&& InHandler)
			: Pending(InPending)
			, Handler(MoveTemp(InHandler))
		{
		}

		FPendingList* Pending;
		FHandler Handler;
		std::atomic<bool> bCancelled{false};
	};

	/** Steam callback thread. Copy and dispatch only. Must not touch members after Enqueue(). */
	void OnRawResult(TPayload* Payload, bool bIOFailure)
	{
		TSharedRef<FState, ESPMode::ThreadSafe> LocalState = State;
		TPayload PayloadCopy = *Payload;

		Dispatcher->Enqueue([LocalState, PayloadCopy, bIOFailure]()
		{
			if (LocalState->bCancelled.load(std::memory_order_acquire))
			{
				return;
			}

			LocalState->Handler(PayloadCopy, bIOFailure);

			// The handler may have cleared the list, which sets bCancelled on this state and destroys the item.
			if (!LocalState->bCancelled.load(std::memory_order_acquire) && LocalState->Pending)
			{
				LocalState->Pending->RemoveAll([&LocalState](const TUniquePtr<TSteamCallResult>& Item)
				{
					return Item->State == LocalState;
				});
			}
		});
	}

	TSharedRef<FState, ESPMode::ThreadSafe> State;
	FDispatcherRef Dispatcher;
	CCallResult<TSteamCallResult, TPayload> CallResult;
};

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
