// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Validation/SteamProjectValidator.h"

struct FSlateBrush;

/** Status dot in front of a row or the banner. Maps to the SandwichSteam.Status.* brushes. */
enum class ESteamConfirmMark : uint8
{
	None,
	Ok,
	Info,
	Warning,
	Error,
	/** Grey: will not happen / not used (e.g. a skipped step). */
	Idle
};

/** One "label: value" line. Note is a smaller, subdued line under the value (paths, consequences, how to change it). */
struct FSteamConfirmRow
{
	FText Label;
	FText Value;
	FText Note;
	ESteamConfirmMark Mark = ESteamConfirmMark::None;
};

/** A titled card of rows. */
struct FSteamConfirmSection
{
	FText Title;
	TArray<FSteamConfirmRow> Rows;

	FSteamConfirmSection& AddRow(const FText& Label, const FText& Value, const FText& Note = FText::GetEmpty(), ESteamConfirmMark Mark = ESteamConfirmMark::None)
	{
		Rows.Add({Label, Value, Note, Mark});
		return *this;
	}
};

/** A checkbox at the bottom. A required option must be checked before Confirm is enabled (an "I understand" acknowledgement). */
struct FSteamConfirmOption
{
	FName Id;
	FText Label;
	FText ToolTip;
	bool bInitiallyChecked = false;
	bool bRequired = false;
};

/**
 * Everything a confirm window shows. Pure data: any dashboard action (Publish, Fetch App Info, Configure Steam, ...) fills one
 * of these and calls SandwichSteam::Editor::ShowConfirmDialog. Empty fields are not drawn.
 */
struct FSteamConfirmRequest
{
	/** Window title and heading. */
	FText Title;
	/** One or two sentences under the heading: what the action does. */
	FText Intro;

	/** Highlighted one-line summary (the most important consequence). Not drawn when BannerText is empty. */
	ESteamConfirmMark BannerMark = ESteamConfirmMark::Info;
	FText BannerText;

	TArray<FSteamConfirmSection> Sections;
	TArray<FSteamConfirmOption> Options;

	/** Small subdued line above the buttons. */
	FText Footer;

	/** When set, shown above the buttons with an error dot and Confirm stays disabled (the user can still read everything and cancel). */
	FText BlockedReason;

	/** Defaults to "Confirm" / "Cancel". */
	FText ConfirmLabel;
	FText CancelLabel;

	/** Heading icon. Null = the plugin icon. */
	const FSlateBrush* Icon = nullptr;

	FSteamConfirmSection& AddSection(const FText& SectionTitle)
	{
		FSteamConfirmSection& Section = Sections.AddDefaulted_GetRef();
		Section.Title = SectionTitle;
		return Section;
	}
};

struct FSteamConfirmResult
{
	bool bConfirmed = false;
	/** State of every option by Id when the window closed. */
	TMap<FName, bool> Options;

	bool IsOptionChecked(FName Id) const
	{
		const bool* Value = Options.Find(Id);
		return Value && *Value;
	}
};

namespace SandwichSteam::Editor
{
	/** Opens the confirm window modally (game thread) and returns when it closes. Escape or closing the window = cancelled. */
	FSteamConfirmResult ShowConfirmDialog(const FSteamConfirmRequest& Request);

	ESteamConfirmMark ToConfirmMark(ESteamCheckSeverity Severity);

	/**
	 * A section listing validation checks (label: detail, with their dot). bIssuesOnly keeps warnings and errors only;
	 * when nothing is left, one "All checks passed" row is added instead.
	 */
	FSteamConfirmSection MakeChecksSection(const FText& Title, const TArray<FSteamValidationCheck>& Checks, bool bIssuesOnly);
}
