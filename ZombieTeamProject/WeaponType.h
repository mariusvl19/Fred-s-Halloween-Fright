#pragma once
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	EWT_AK47 UMETA(DisplayName = "AK47"),
	EWT_AssaultRifle UMETA(DisplayName = "AssaultRifle"),

	EWT_MAX UMETA(DisplayName = "DefaultMAX")
};

