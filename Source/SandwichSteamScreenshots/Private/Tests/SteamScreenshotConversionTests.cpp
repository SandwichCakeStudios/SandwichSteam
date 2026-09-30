// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamScreenshotConversion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamScreenshotConversionTest, "SandwichSteam.Screenshots.Conversion.BgraToRgb",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamScreenshotConversionTest::RunTest(const FString& Parameters)
{
	// FColor(R, G, B, A) is stored B G R A. Steam wants R G B, tightly packed.
	const TArray<FColor> Pixels = { FColor(1, 2, 3, 255), FColor(10, 20, 30, 0), FColor(255, 128, 0, 7) };

	TArray<uint8> Rgb;
	SandwichSteam::Screenshots::ConvertBgraToRgb(Pixels, Rgb);

	const TArray<uint8> Expected = { 1, 2, 3, 10, 20, 30, 255, 128, 0 };
	TestEqual(TEXT("3 bytes per pixel"), Rgb.Num(), 9);
	TestTrue(TEXT("R G B order, alpha dropped"), Rgb == Expected);

	TArray<uint8> Empty;
	SandwichSteam::Screenshots::ConvertBgraToRgb(TConstArrayView<FColor>(), Empty);
	TestEqual(TEXT("No pixels, no bytes"), Empty.Num(), 0);

	// A whole frame: every pixel of a 1920x1080 bitmap converts and the last one is right.
	TArray<FColor> Frame;
	Frame.Init(FColor(9, 8, 7, 6), 1920 * 1080);
	SandwichSteam::Screenshots::ConvertBgraToRgb(Frame, Rgb);
	TestEqual(TEXT("Full frame size"), Rgb.Num(), 1920 * 1080 * 3);
	TestTrue(TEXT("Last pixel"), Rgb.Last(2) == 9 && Rgb.Last(1) == 8 && Rgb.Last() == 7);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
