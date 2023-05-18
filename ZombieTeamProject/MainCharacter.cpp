// Marius Vlad

#include "MainCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Particles/ParticleSystemComponent.h"
#include "Item.h"
#include "Components/WidgetComponent.h"
#include "Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Ammo.h"
#include "BulletHitInterface.h"
#include "Enemy.h"
#include "EnemyController.h"
#include "BehaviorTree/BlackboardComponent.h"

//////////////////////////////////////////////////////////////////////////
// AMainCharacter

AMainCharacter::AMainCharacter() :
	/** Initializer list */
	// Set our turn rates for input
	BaseTurnRate(45.f),
	BaseLookUpRate(45.f),
	//Set the aiming
	isAiming(false),
	//Set the camera
	CameraDefaultFOV(0.f),
	CameraZoomedFOV(35.f),
	//Gun fire rate
	fireRate(0.1f),
	charShouldFire(true),
	isFireButtonPressed(false),
	charShouldTraceForItems(false),
	//Ammo amounts
	Starting9mmAmmo(85),
	StartingARAmmo(120),
	//Combat variables
	CombatState(ECombatState::ECS_Unoccupied),
	//Camera interp
	CameraInterpDistance(250.f),
	CameraInterpElevation(65.f),
	//Character health
	Health(100.f),
	MaxHealth(100.f)

{

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);



	PrimaryActorTick.bCanEverTick = true;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 180.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 70.f);
	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
}

float AMainCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (Health - DamageAmount <= 0.f)
	{
		Health = 0.f;
		MainCharacterDeath();

		auto EnemyController = Cast<AEnemyController>(EventInstigator);
		if (EnemyController)
		{
			EnemyController->GetBlackboardComponent()->SetValueAsBool(FName(TEXT("MainCharacterDead")), true);
		}
	}
	else
	{
		Health -= DamageAmount;
	}
	return DamageAmount;
}

//////////////////////////////////////////////////////////////////////////
// Input



void AMainCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);
	// Set up gameplay key bindings
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AMainCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMainCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AMainCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AMainCharacter::LookUpAtRate);

	PlayerInputComponent->BindAction("FireButton", IE_Pressed, this, &AMainCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("FireButton", IE_Released, this, &AMainCharacter::FireButtonReleased);

	PlayerInputComponent->BindAction("AimingButton", IE_Pressed, this, &AMainCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("AimingButton", IE_Released, this, &AMainCharacter::AimingButtonReleased);


	PlayerInputComponent->BindAction("InteractButton", IE_Pressed, this, &AMainCharacter::InteractButtonPressed);
	PlayerInputComponent->BindAction("InteractButton", IE_Released, this, &AMainCharacter::InteractButtonReleased);

	PlayerInputComponent->BindAction("ReloadButton", IE_Pressed, this, &AMainCharacter::ReloadButtonPressed);

	PlayerInputComponent->BindAction("FKey", IE_Pressed, this, &AMainCharacter::FKeyPressed);
	PlayerInputComponent->BindAction("1Key", IE_Pressed, this, &AMainCharacter::OneKeyPressed);
	PlayerInputComponent->BindAction("2Key", IE_Pressed, this, &AMainCharacter::TwoKeyPressed);
	PlayerInputComponent->BindAction("3Key", IE_Pressed, this, &AMainCharacter::ThreeKeyPressed);
	PlayerInputComponent->BindAction("4Key", IE_Pressed, this, &AMainCharacter::FourKeyPressed);
	PlayerInputComponent->BindAction("5Key", IE_Pressed, this, &AMainCharacter::FiveKeyPressed);
}



void AMainCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FollowCamera)
	{
		CameraDefaultFOV = GetFollowCamera()->FieldOfView;
	}

	//This function will spawn the weapon and equip it to the main character
	Equip(SpawnDefaultWeapon());
	Inventory.Add(EquipWeapon);
	EquipWeapon->SetSlotIndex(0);
	InitializeAmmoMap();
}

void AMainCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AMainCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AMainCharacter::FireWeapon()
{
	if (EquipWeapon == nullptr) return;

	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (WeaponHasAmmo())
	{
		PlayFireSound();
		SendBullet();
		PlayGunFireMontage();
		EquipWeapon->DecrementAmmo();

		StartFireTimer();
	}

}

void AMainCharacter::AimingButtonPressed()
{
	isAiming = true;
	GetFollowCamera()->SetFieldOfView(CameraZoomedFOV);
}

void AMainCharacter::AimingButtonReleased()
{
	isAiming = false;
	GetFollowCamera()->SetFieldOfView(CameraDefaultFOV);
}

void AMainCharacter::FireButtonPressed()
{
	isFireButtonPressed = true;
	FireWeapon();
}

void AMainCharacter::FireButtonReleased()
{
	isFireButtonPressed = false;

}

void AMainCharacter::StartFireTimer()
{
	CombatState = ECombatState::ECS_FireTimerInProgress;

	GetWorldTimerManager().SetTimer(
		FireTimer,
		this,
		&AMainCharacter::AutoFireReset,
		fireRate);

}

void AMainCharacter::AutoFireReset()
{
	CombatState = ECombatState::ECS_Unoccupied;

	if (WeaponHasAmmo())
	{
		if (isFireButtonPressed)
		{
			FireWeapon();
		}
	}
	else
	{
		ReloadWeapon();
	}
}

bool AMainCharacter::TraceWidgetUnderCrosshair(FHitResult& OutHitResult, FVector& OutHitLocation)
{
	//Get current size of the viewport
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	//Get screen space location of crosshair
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	CrosshairLocation.Y -= 50.f;
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirectition;

	//Get world position and direction of crosshair
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirectition);

	if (bScreenToWorld)
	{
		const FVector Start{ CrosshairWorldPosition };
		const FVector End{ Start + CrosshairWorldDirectition * 50'000.f };
		OutHitLocation = End;

		GetWorld()->LineTraceSingleByChannel(OutHitResult, Start, End,
			ECollisionChannel::ECC_Visibility);
		if (OutHitResult.bBlockingHit)
		{
			OutHitLocation = OutHitResult.Location;
			return true;
		}
	}

	return false;
}

bool AMainCharacter::GetBeamEndLocation(
	const FVector& MuzzleSocketLocation,
	FHitResult& OutHitResult)
{
	FVector OutBeamLocation;
	// Check for crosshair trace hit
	FHitResult CrosshairHitResult;
	bool bCrosshairHit = TraceWidgetUnderCrosshair(CrosshairHitResult, OutBeamLocation);

	if (bCrosshairHit)
	{
		// Tentative beam location - still need to trace from gun
		OutBeamLocation = CrosshairHitResult.Location;
	}
	else // no crosshair trace hit
	{
		// OutBeamLocation is the End location for the line trace
	}

	// Perform a second trace, this time from the gun barrel
	const FVector WeaponTraceStart{ MuzzleSocketLocation };
	const FVector StartToEnd{ OutBeamLocation - WeaponTraceStart };
	const FVector WeaponTraceEnd{ MuzzleSocketLocation + StartToEnd * 1.25f };
	GetWorld()->LineTraceSingleByChannel(
		OutHitResult,
		WeaponTraceStart,
		WeaponTraceEnd,
		ECollisionChannel::ECC_Visibility);
	if (!OutHitResult.bBlockingHit) // object between barrel and BeamEndPoint?
	{
		OutHitResult.Location = OutBeamLocation;
		return false;
	}

	return true;
}

void AMainCharacter::TraceForItems()
{
	if (charShouldTraceForItems)
	{
		FHitResult ItemTraceResult;
		FVector HitLocation;
		TraceWidgetUnderCrosshair(ItemTraceResult, HitLocation);

		if (ItemTraceResult.bBlockingHit)
		{
			TraceHitItem = Cast<AItem>(ItemTraceResult.GetActor());

			if (TraceHitItem && TraceHitItem->GetPickupWidget())
			{
				//Make pickup widget visible
				TraceHitItem->GetPickupWidget()->SetVisibility(true);
			}

			if (TraceHitItemLastFrame)
			{
				if (TraceHitItem != TraceHitItemLastFrame)
				{
					TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
				}
			}

			TraceHitItemLastFrame = TraceHitItem;
		}
	}

	else if (TraceHitItemLastFrame)
	{
		TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
	}
}

AWeapon* AMainCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass)
	{
		//Handles spawning the weapon in the world
		return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);
	}

	return nullptr;
}

void AMainCharacter::Equip(AWeapon* EquippableWeapon)
{
	if (EquippableWeapon)
	{
		const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));
		if (HandSocket)
		{
			HandSocket->AttachActor(EquippableWeapon, GetMesh());
		}

		EquipWeapon = EquippableWeapon;
		EquipWeapon->SetItemState(EItemState::EIS_Equipped);

	}
}

void AMainCharacter::DropWeapon()
{
	if (EquipWeapon)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
		EquipWeapon->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);

		EquipWeapon->SetItemState(EItemState::EIS_Falling);
	}
}

void AMainCharacter::InteractButtonPressed()
{

	if (TraceHitItem)
	{
		TraceHitItem->StartItemCurve(this);

		if (TraceHitItem->GetPickupSound())
		{
			UGameplayStatics::PlaySound2D(this, TraceHitItem->GetPickupSound());
		}
	}

}

void AMainCharacter::InteractButtonReleased()
{

}

void AMainCharacter::SwapWeapon(AWeapon* WeaponToSwap)
{
	if (Inventory.Num() - 1 >= EquipWeapon->GetSlotIndex())
	{
		Inventory[EquipWeapon->GetSlotIndex()] = WeaponToSwap;
	}

	DropWeapon();
	Equip(WeaponToSwap);
	TraceHitItem = nullptr;
	TraceHitItemLastFrame = nullptr;
}

void AMainCharacter::InitializeAmmoMap()
{
	AmmoMap.Add(EAmmoType::EAT_9mm, Starting9mmAmmo);
	AmmoMap.Add(EAmmoType::EAT_AR, StartingARAmmo);
}

bool AMainCharacter::WeaponHasAmmo()
{
	if (EquipWeapon == nullptr) return false;

	return EquipWeapon->GetAmmo() > 0;
}

void AMainCharacter::FKeyPressed()
{
	if (EquipWeapon->GetSlotIndex() == 0) return;
	ExchangeInventoryItems(EquipWeapon->GetSlotIndex(), 0);
}

void AMainCharacter::OneKeyPressed()
{
	if (EquipWeapon->GetSlotIndex() == 1) return;
	ExchangeInventoryItems(EquipWeapon->GetSlotIndex(), 1);
}

void AMainCharacter::TwoKeyPressed()
{
	if (EquipWeapon->GetSlotIndex() == 2) return;
	ExchangeInventoryItems(EquipWeapon->GetSlotIndex(), 2);
}

void AMainCharacter::ThreeKeyPressed()
{
	if (EquipWeapon->GetSlotIndex() == 3) return;
	ExchangeInventoryItems(EquipWeapon->GetSlotIndex(), 3);
}

void AMainCharacter::FourKeyPressed()
{
	if (EquipWeapon->GetSlotIndex() == 4) return;
	ExchangeInventoryItems(EquipWeapon->GetSlotIndex(), 4);
}

void AMainCharacter::FiveKeyPressed()
{
	if (EquipWeapon->GetSlotIndex() == 5) return;
	ExchangeInventoryItems(EquipWeapon->GetSlotIndex(), 5);
}

void AMainCharacter::ExchangeInventoryItems(int32 CurrentItemIndex, int32 NewItemIndex)
{
	if ((CurrentItemIndex == NewItemIndex) || (NewItemIndex >= Inventory.Num())) return;
	auto OldEquippedWeapon = EquipWeapon;
	auto NewWeapon = Cast<AWeapon>(Inventory[NewItemIndex]);
	Equip(NewWeapon);

	OldEquippedWeapon->SetItemState(EItemState::EIS_PickedUp);
	NewWeapon->SetItemState(EItemState::EIS_Equipped);
}

void AMainCharacter::PlayFireSound()
{
	//Play FireSound
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}
}

void AMainCharacter::SendBullet()
{
	// Send bullet
	const USkeletalMeshSocket* BarrelSocket =
		EquipWeapon->GetItemMesh()->GetSocketByName("BarrelSocket");
	if (BarrelSocket)
	{
		const FTransform SocketTransform = BarrelSocket->GetSocketTransform(
			EquipWeapon->GetItemMesh());

		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
		}

		FHitResult BeamHitResult;
		bool bBeamEnd = GetBeamEndLocation(
			SocketTransform.GetLocation(), BeamHitResult);
		if (bBeamEnd)
		{
			// Does hit Actor implement BulletHitInterface?
			if (BeamHitResult.GetActor())
			{
				IBulletHitInterface* BulletHitInterface = Cast<IBulletHitInterface>(BeamHitResult.GetActor());
				if (BulletHitInterface)
				{
					BulletHitInterface->BulletHit_Implementation(BeamHitResult);
				}

				AEnemy* HitEnemy = Cast<AEnemy>(BeamHitResult.GetActor());

				if (HitEnemy)
				{
					int32 Damage{};
					if (BeamHitResult.BoneName.ToString() == HitEnemy->GetHeadBone())
					{
						//Head shot
						Damage = EquipWeapon->GetHeadShotDamage();
						UGameplayStatics::ApplyDamage(BeamHitResult.GetActor(),
							Damage,
							GetController(),
							this,
							UDamageType::StaticClass());
					}
					else
					{
						//Body shot
						Damage = EquipWeapon->GetDamage();
						UGameplayStatics::ApplyDamage(BeamHitResult.GetActor(),
							Damage,
							GetController(),
							this,
							UDamageType::StaticClass());
					}

					HitEnemy->ShowHitNumber(Damage, BeamHitResult.Location);

				}
			}
			else
			{
				// Spawn default particles
				if (ImpactParticles)
				{
					UGameplayStatics::SpawnEmitterAtLocation(
						GetWorld(),
						ImpactParticles,
						BeamHitResult.Location);
				}
			}

			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				BeamParticles,
				SocketTransform);
			if (Beam)
			{
				Beam->SetVectorParameter(FName("Target"), BeamHitResult.Location);
			}
		}
	}
}

void AMainCharacter::PlayGunFireMontage()
{
	//PlayHipFireMontage
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}
}

void AMainCharacter::ReloadButtonPressed()
{
	ReloadWeapon();
}

void AMainCharacter::ReloadWeapon()
{
	if (CombatState != ECombatState::ECS_Unoccupied) return;
	if (EquipWeapon == nullptr) return;

	if (CarriedAmmo())
	{
		CombatState = ECombatState::ECS_Reloading;

		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && ReloadMontage)
		{
			AnimInstance->Montage_Play(ReloadMontage);
			AnimInstance->Montage_JumpToSection(EquipWeapon->GetReloadMontageSection());
		}
	}
}

bool AMainCharacter::CarriedAmmo()
{
	if (EquipWeapon == nullptr) return false;

	auto AmmoType = EquipWeapon->GetAmmoType();

	if (AmmoMap.Contains(AmmoType))
	{
		return AmmoMap[AmmoType] > 0;
	}
	return false;
}

void AMainCharacter::PickupAmmo(AAmmo* Ammo)
{
	if (AmmoMap.Find(Ammo->GetAmmoType()))
	{
		int32 AmmoCount{ AmmoMap[Ammo->GetAmmoType()] };
		AmmoCount += Ammo->GetItemCount();
		AmmoMap[Ammo->GetAmmoType()] = AmmoCount;
	}

	if (EquipWeapon->GetAmmoType() == Ammo->GetAmmoType())
	{
		if (EquipWeapon->GetAmmo() == 0)
		{
			ReloadWeapon();
		}
	}

	Ammo->Destroy();
}

void AMainCharacter::MainCharacterDeath()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && DeathMontage)
	{
		AnimInstance->Montage_Play(DeathMontage);
		AnimInstance->Montage_JumpToSection(FName("DeathA"), DeathMontage);
	}
	
}

void AMainCharacter::FinishDeath()
{
	GetMesh()->bPauseAnims = true;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		DisableInput(PC);
	}
}



void AMainCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AMainCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AMainCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMainCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}



void AMainCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//Check OverlappedItemCount and then trace for items
	TraceForItems();
}

void AMainCharacter::FinishReloading()
{
	CombatState = ECombatState::ECS_Unoccupied;

	if (EquipWeapon == nullptr) return;

	const auto AmmoType{ EquipWeapon->GetAmmoType() };

	if (AmmoMap.Contains(AmmoType))
	{
		int32 CarriedAmmo = AmmoMap[AmmoType];

		const int32 MagazineEmptySpace = EquipWeapon->GetMagazineCapacity() -
			EquipWeapon->GetAmmo();

		if (MagazineEmptySpace > CarriedAmmo)
		{
			EquipWeapon->ReloadAmmo(CarriedAmmo);
			CarriedAmmo = 0;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
		else
		{
			EquipWeapon->ReloadAmmo(MagazineEmptySpace);
			CarriedAmmo -= MagazineEmptySpace;
			AmmoMap.Add(AmmoType, CarriedAmmo);
		}
	}
}

void AMainCharacter::IncrementOverlappedItemCount(int8 Amount)
{
	if (OverlappedItemCount + Amount <= 0)
	{
		OverlappedItemCount = 0;
		charShouldTraceForItems = false;
	}

	else
	{
		OverlappedItemCount += Amount;
		charShouldTraceForItems = true;
	}
}

FVector AMainCharacter::GetCameraInterpLocation()
{
	const FVector CameraWorldLocation{ FollowCamera->GetComponentLocation() };
	const FVector CameraForward{ FollowCamera->GetForwardVector() };

	return CameraWorldLocation + CameraForward * CameraInterpDistance
		+ FVector(0.f, 0.f, CameraInterpElevation);
}

void AMainCharacter::GetPickupItem(AItem* Item)
{

	auto Weapon = Cast<AWeapon>(Item);
	if (Weapon)
	{
		if (Inventory.Num() < INVENTORY_CAPACITY)
		{
			Weapon->SetSlotIndex(Inventory.Num());
			Inventory.Add(Weapon);
			Weapon->SetItemState(EItemState::EIS_PickedUp);
		}
		else
		{
			SwapWeapon(Weapon);
		}
	}

	auto Ammo = Cast<AAmmo>(Item);
	if (Ammo)
	{
		PickupAmmo(Ammo);
	}

}









