// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZombieTeamProjectGameMode.h"
#include "ZombieTeamProjectCharacter.h"
#include "UObject/ConstructorHelpers.h"

AZombieTeamProjectGameMode::AZombieTeamProjectGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
