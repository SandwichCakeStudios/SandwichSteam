// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "SteamAppDefinitionFactory.generated.h"

/** Creates Steam App Definition assets (Content Browser > Add > Steam > Steam App Definition). */
UCLASS()
class USteamAppDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	USteamAppDefinitionFactory();

	//~ Begin UFactory
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	//~ End UFactory
};
