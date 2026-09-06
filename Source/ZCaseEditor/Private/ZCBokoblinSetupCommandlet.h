#pragma once

#include "Commandlets/Commandlet.h"
#include "ZCBokoblinSetupCommandlet.generated.h"

/** Builds the small, editable Bokoblin demo asset set using the engine asset APIs. */
UCLASS()
class UZCBokoblinSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UZCBokoblinSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
