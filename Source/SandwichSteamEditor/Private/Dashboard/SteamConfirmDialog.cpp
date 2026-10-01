// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Dashboard/SteamConfirmDialog.h"
#include "Editor.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamConfirmDialog"

namespace
{
	constexpr float WindowWidth = 680.f;
	constexpr float MaxBodyHeight = 520.f;
	constexpr float LabelWidth = 180.f;
	constexpr float DotSize = 10.f;
	constexpr float BannerDotSize = 14.f;

	FSlateFontInfo BodyFont() { return FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f); }
	FSlateFontInfo NoteFont() { return FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.1f); }
	FSlateFontInfo SectionFont() { return FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.3f); }

	const FSlateBrush* GetMarkBrush(ESteamConfirmMark Mark)
	{
		switch (Mark)
		{
		case ESteamConfirmMark::Ok:			return FSteamToolStyle::Get().GetBrush("SandwichSteam.Status.Ok");
		case ESteamConfirmMark::Info:		return FSteamToolStyle::Get().GetBrush("SandwichSteam.Status.Info");
		case ESteamConfirmMark::Warning:	return FSteamToolStyle::Get().GetBrush("SandwichSteam.Status.Warning");
		case ESteamConfirmMark::Error:		return FSteamToolStyle::Get().GetBrush("SandwichSteam.Status.Error");
		case ESteamConfirmMark::Idle:		return FSteamToolStyle::Get().GetBrush("SandwichSteam.Status.Idle");
		default:							return nullptr;
		}
	}

	/** Fixed-size dot, or an empty box of the same size so rows with and without a mark stay aligned. */
	TSharedRef<SWidget> MakeDot(ESteamConfirmMark Mark, float Size)
	{
		const FSlateBrush* Brush = GetMarkBrush(Mark);
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
		[
			Brush ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Brush)) : SNullWidget::NullWidget
		];
	}

	TSharedRef<SWidget> MakeRow(const FSteamConfirmRow& Row, bool bAnyMarks)
	{
		const TSharedRef<SVerticalBox> ValueBox = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Row.Value).Font(BodyFont()).AutoWrapText(true)
			];
		if (!Row.Note.IsEmpty())
		{
			ValueBox->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Row.Note).Font(NoteFont()).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}

		const TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
		if (bAnyMarks)
		{
			Line->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 5.f, 10.f, 0.f)
			[
				MakeDot(Row.Mark, DotSize)
			];
		}
		Line->AddSlot().AutoWidth().VAlign(VAlign_Top)
		[
			SNew(SBox).WidthOverride(LabelWidth)
			[
				SNew(STextBlock).Text(Row.Label).Font(BodyFont()).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
		Line->AddSlot().FillWidth(1.f).VAlign(VAlign_Top).Padding(12.f, 0.f, 0.f, 0.f)
		[
			ValueBox
		];
		return Line;
	}

	TSharedRef<SWidget> MakeSection(const FSteamConfirmSection& Section)
	{
		const bool bAnyMarks = Section.Rows.ContainsByPredicate([](const FSteamConfirmRow& Row) { return Row.Mark != ESteamConfirmMark::None; });

		const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		for (int32 Index = 0; Index < Section.Rows.Num(); ++Index)
		{
			Rows->AddSlot().AutoHeight().Padding(0.f, Index == 0 ? 0.f : 8.f, 0.f, 0.f)
			[
				MakeRow(Section.Rows[Index], bAnyMarks)
			];
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock).Text(Section.Title).Font(SectionFont()).Visibility(Section.Title.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(12.f)
				[
					Rows
				]
			];
	}
}

namespace SandwichSteam::Editor
{
	ESteamConfirmMark ToConfirmMark(ESteamCheckSeverity Severity)
	{
		switch (Severity)
		{
		case ESteamCheckSeverity::Error:	return ESteamConfirmMark::Error;
		case ESteamCheckSeverity::Warning:	return ESteamConfirmMark::Warning;
		case ESteamCheckSeverity::Info:		return ESteamConfirmMark::Info;
		default:							return ESteamConfirmMark::Ok;
		}
	}

	FSteamConfirmSection MakeChecksSection(const FText& Title, const TArray<FSteamValidationCheck>& Checks, bool bIssuesOnly)
	{
		FSteamConfirmSection Section;
		Section.Title = Title;
		for (const FSteamValidationCheck& Check : Checks)
		{
			if (!bIssuesOnly || Check.IsIssue())
			{
				Section.AddRow(Check.Label, Check.Detail, FText::GetEmpty(), ToConfirmMark(Check.Severity));
			}
		}
		if (Section.Rows.IsEmpty())
		{
			Section.AddRow(LOCTEXT("ChecksLabel", "Checks"), LOCTEXT("ChecksPassed", "All checks passed."), FText::GetEmpty(), ESteamConfirmMark::Ok);
		}
		return Section;
	}

	FSteamConfirmResult ShowConfirmDialog(const FSteamConfirmRequest& Request)
	{
		check(IsInGameThread());

		const TSharedRef<FSteamConfirmResult> Result = MakeShared<FSteamConfirmResult>();
		for (const FSteamConfirmOption& Option : Request.Options)
		{
			Result->Options.Add(Option.Id, Option.bInitiallyChecked);
		}

		const TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Request.Title)
			.SizingRule(ESizingRule::Autosized)
			.SupportsMaximize(false)
			.SupportsMinimize(false);
		const TWeakPtr<SWindow> WeakWindow = Window;

		auto CanConfirm = [Result, Options = Request.Options, bBlocked = !Request.BlockedReason.IsEmpty()]()
		{
			if (bBlocked)
			{
				return false;
			}
			for (const FSteamConfirmOption& Option : Options)
			{
				if (Option.bRequired && !Result->IsOptionChecked(Option.Id))
				{
					return false;
				}
			}
			return true;
		};

		// Body: intro, banner, sections. Scrolls when long so the buttons always stay on screen.
		const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		if (!Request.Intro.IsEmpty())
		{
			Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(STextBlock).Text(Request.Intro).Font(BodyFont()).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		}
		if (!Request.BannerText.IsEmpty())
		{
			Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(FMargin(12.f, 10.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
					[
						MakeDot(Request.BannerMark, BannerDotSize)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Request.BannerText).Font(SectionFont()).AutoWrapText(true)
					]
				]
			];
		}
		for (const FSteamConfirmSection& Section : Request.Sections)
		{
			Body->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				MakeSection(Section)
			];
		}

		// Options (checkboxes), left of the buttons' row.
		const TSharedRef<SVerticalBox> OptionsBox = SNew(SVerticalBox);
		for (const FSteamConfirmOption& Option : Request.Options)
		{
			const FName Id = Option.Id;
			OptionsBox->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(SCheckBox)
				.ToolTipText(Option.ToolTip)
				.IsChecked_Lambda([Result, Id]() { return Result->IsOptionChecked(Id) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Result, Id](ECheckBoxState State) { Result->Options.Add(Id, State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(Option.Label).Font(BodyFont()).AutoWrapText(true)
				]
			];
		}

		const FText ConfirmLabel = Request.ConfirmLabel.IsEmpty() ? LOCTEXT("Confirm", "Confirm") : Request.ConfirmLabel;
		const FText CancelLabel = Request.CancelLabel.IsEmpty() ? LOCTEXT("Cancel", "Cancel") : Request.CancelLabel;
		const FSlateBrush* Icon = Request.Icon ? Request.Icon : FSteamToolStyle::Get().GetBrush("SandwichSteam.Icon40");

		Window->SetContent(
			SNew(SBorder).BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel")).Padding(18.f)
			[
				SNew(SBox).WidthOverride(WindowWidth)
				[
					SNew(SVerticalBox)
					// Heading
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
						[
							SNew(SImage).Image(Icon)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(Request.Title).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("HeadingExtraSmallText"), 1.5f))
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).MaxDesiredHeight(MaxBodyHeight)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								Body
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						SNew(STextBlock).Text(Request.Footer).Font(NoteFont()).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Visibility(Request.Footer.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
					]
					// Options on their own full-width row, so long labels wrap instead of running under the buttons.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						SNew(SBorder)
						.BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card"))
						.Padding(FMargin(12.f, 8.f, 12.f, 4.f))
						.Visibility(Request.Options.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						[
							OptionsBox
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
					[
						SNew(SHorizontalBox)
						.Visibility(Request.BlockedReason.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 4.f, 10.f, 0.f)
						[
							MakeDot(ESteamConfirmMark::Error, DotSize)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(STextBlock).Text(Request.BlockedReason).Font(BodyFont()).AutoWrapText(true)
						]
					]
					// Cancel, Confirm (right aligned)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0.f, 0.f, 6.f, 0.f)
						[
							SNew(SButton)
							.ContentPadding(FMargin(14.f, 5.f))
							.OnClicked_Lambda([WeakWindow]()
							{
								if (const TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
								{
									Pinned->RequestDestroyWindow();
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Text(CancelLabel).Font(BodyFont())
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
						[
							SNew(SButton)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
							.ContentPadding(FMargin(14.f, 5.f))
							.IsEnabled_Lambda(CanConfirm)
							.ToolTipText_Lambda([CanConfirm, BlockedReason = Request.BlockedReason]()
							{
								if (CanConfirm())
								{
									return FText::GetEmpty();
								}
								return BlockedReason.IsEmpty() ? LOCTEXT("RequiredTip", "Check the box above first.") : BlockedReason;
							})
							.OnClicked_Lambda([Result, WeakWindow]()
							{
								Result->bConfirmed = true;
								if (const TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
								{
									Pinned->RequestDestroyWindow();
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Text(ConfirmLabel).Font(BodyFont())
							]
						]
					]
				]
			]);

		GEditor->EditorAddModalWindow(Window);
		return *Result;
	}
}

#undef LOCTEXT_NAMESPACE
