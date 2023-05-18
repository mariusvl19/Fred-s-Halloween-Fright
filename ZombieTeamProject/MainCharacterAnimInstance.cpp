// Fill out your copyright notice in the Description page of Project Settings.


#include "MainCharacterAnimInstance.h"
#include "MainCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon.h"
#include "WeaponType.h"

void UMainCharacterAnimInstance::UpdateAnimationProperties(float DeltaTime)
{
	if (MainCharacter == nullptr)
	{
		MainCharacter = Cast<AMainCharacter>(TryGetPawnOwner());
	}

	if (MainCharacter)
	{
		FVector Velocity = { MainCharacter->GetVelocity() };
		Velocity.Z = 0;
		charSpeed = Velocity.Size();

		charIsInAir = MainCharacter->GetCharacterMovement()->IsFalling();

		if (MainCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f)
		{
			charIsAccelerating = true;
		}

		else
		{
			charIsAccelerating = false;
		}

		isAiming = MainCharacter->GetAiming();
	}

	/*if (MainCharacter->GetEquipWeapon())
	{
		EquippedWeaponType = MainCharacter->GetEquipWeapon()->GetWeaponType();
	}*/
}

void UMainCharacterAnimInstance::NativeInitializeAnimation()
{
	MainCharacter = Cast<AMainCharacter>(TryGetPawnOwner());
}
