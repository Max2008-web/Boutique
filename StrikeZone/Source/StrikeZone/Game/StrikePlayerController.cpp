// STRIKE ZONE - contrôleur joueur : clavier/souris, manette, tactile, menus, réglages
#include "Game/StrikePlayerController.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerState.h"
#include "Character/StrikeCharacter.h"
#include "Core/StrikeGameInstance.h"
#include "UI/StrikeHUD.h"
#include "UI/StrikeTouchControls.h"
#include "Weapons/StrikeWeapon.h"
#include "StrikeZone.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"

AStrikePlayerController::AStrikePlayerController()
{
	bShowMouseCursor = false;
}

AStrikeCharacter* AStrikePlayerController::GetStrikeCharacter() const
{
	return Cast<AStrikeCharacter>(GetPawn());
}

bool AStrikePlayerController::IsMenuMode() const
{
	const AStrikeGameState* GS = GetWorld() ? GetWorld()->GetGameState<AStrikeGameState>() : nullptr;
	return GS && GS->ModeType == EStrikeModeType::Menu;
}

bool AStrikePlayerController::IsUsingTouch() const
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#else
	return Settings.bForceTouchControls || (TouchControls && TouchControls->HasSeenTouch());
#endif
}

void AStrikePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (const UStrikeGameInstance* GI = GetGameInstance<UStrikeGameInstance>())
	{
		Settings = GI->GetSettings();
	}

	TouchControls = NewObject<UStrikeTouchControls>(this);

	// Désactive le joystick virtuel par défaut du moteur : nous dessinons le nôtre.
	ActivateTouchInterface(nullptr);
	RefreshInputMode();
}

void AStrikePlayerController::RefreshInputMode()
{
	const bool bMenu = IsMenuMode();
	if (bInputModeInitialized && bMenu == bMenuModeApplied)
	{
		return;
	}
	bInputModeInitialized = true;
	bMenuModeApplied = bMenu;

	if (bMenu)
	{
		SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		SetShowMouseCursor(false);
		SetInputMode(FInputModeGameOnly());
	}
}

// ---------------------------------------------------------------------------
// Enhanced Input : actions et mapping créés à l'exécution
// ---------------------------------------------------------------------------

void AStrikePlayerController::BuildDefaultInputMappings()
{
	const auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookMouseAction = MakeAction(TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	LookGamepadAction = MakeAction(TEXT("IA_LookGamepad"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	SprintToggleAction = MakeAction(TEXT("IA_SprintToggle"), EInputActionValueType::Boolean);
	FireAction = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	AimAction = MakeAction(TEXT("IA_Aim"), EInputActionValueType::Boolean);
	ReloadAction = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	NextWeaponAction = MakeAction(TEXT("IA_NextWeapon"), EInputActionValueType::Boolean);
	PrevWeaponAction = MakeAction(TEXT("IA_PrevWeapon"), EInputActionValueType::Boolean);
	Weapon1Action = MakeAction(TEXT("IA_Weapon1"), EInputActionValueType::Boolean);
	Weapon2Action = MakeAction(TEXT("IA_Weapon2"), EInputActionValueType::Boolean);
	Weapon3Action = MakeAction(TEXT("IA_Weapon3"), EInputActionValueType::Boolean);
	ScoreboardAction = MakeAction(TEXT("IA_Scoreboard"), EInputActionValueType::Boolean);
	MenuAction = MakeAction(TEXT("IA_Menu"), EInputActionValueType::Boolean);

	RuntimeContext = NewObject<UInputMappingContext>(this, TEXT("IMC_StrikeRuntime"));

	// Déplacement ZQSD/WASD (positions physiques) : Y = avant, X = droite.
	{
		FEnhancedActionKeyMapping& M = RuntimeContext->MapKey(MoveAction, EKeys::W);
		M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	}
	{
		FEnhancedActionKeyMapping& M = RuntimeContext->MapKey(MoveAction, EKeys::S);
		M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	{
		FEnhancedActionKeyMapping& M = RuntimeContext->MapKey(MoveAction, EKeys::A);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	RuntimeContext->MapKey(MoveAction, EKeys::D);
	{
		FEnhancedActionKeyMapping& M = RuntimeContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);
		M.Modifiers.Add(NewObject<UInputModifierDeadZone>(this));
	}

	RuntimeContext->MapKey(LookMouseAction, EKeys::Mouse2D);
	{
		FEnhancedActionKeyMapping& M = RuntimeContext->MapKey(LookGamepadAction, EKeys::Gamepad_Right2D);
		M.Modifiers.Add(NewObject<UInputModifierDeadZone>(this));
	}

	RuntimeContext->MapKey(JumpAction, EKeys::SpaceBar);
	RuntimeContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	RuntimeContext->MapKey(CrouchAction, EKeys::LeftControl);
	RuntimeContext->MapKey(CrouchAction, EKeys::C);
	RuntimeContext->MapKey(CrouchAction, EKeys::Gamepad_FaceButton_Right);
	RuntimeContext->MapKey(SprintAction, EKeys::LeftShift);
	RuntimeContext->MapKey(SprintToggleAction, EKeys::Gamepad_LeftThumbstick);
	RuntimeContext->MapKey(FireAction, EKeys::LeftMouseButton);
	RuntimeContext->MapKey(FireAction, EKeys::Gamepad_RightTrigger);
	RuntimeContext->MapKey(AimAction, EKeys::RightMouseButton);
	RuntimeContext->MapKey(AimAction, EKeys::Gamepad_LeftTrigger);
	RuntimeContext->MapKey(ReloadAction, EKeys::R);
	RuntimeContext->MapKey(ReloadAction, EKeys::Gamepad_FaceButton_Left);
	RuntimeContext->MapKey(InteractAction, EKeys::E);
	RuntimeContext->MapKey(InteractAction, EKeys::F);
	RuntimeContext->MapKey(InteractAction, EKeys::Gamepad_DPad_Up);
	RuntimeContext->MapKey(NextWeaponAction, EKeys::MouseScrollUp);
	RuntimeContext->MapKey(NextWeaponAction, EKeys::Gamepad_FaceButton_Top);
	RuntimeContext->MapKey(PrevWeaponAction, EKeys::MouseScrollDown);
	RuntimeContext->MapKey(Weapon1Action, EKeys::One);
	RuntimeContext->MapKey(Weapon2Action, EKeys::Two);
	RuntimeContext->MapKey(Weapon3Action, EKeys::Three);
	RuntimeContext->MapKey(ScoreboardAction, EKeys::Tab);
	RuntimeContext->MapKey(ScoreboardAction, EKeys::Gamepad_Special_Left);
	RuntimeContext->MapKey(MenuAction, EKeys::Escape);
	RuntimeContext->MapKey(MenuAction, EKeys::P);
	RuntimeContext->MapKey(MenuAction, EKeys::Gamepad_Special_Right);
}

void AStrikePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	BuildDefaultInputMappings();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(RuntimeContext, 0);
			if (DefaultInputContext)
			{
				Subsystem->AddMappingContext(DefaultInputContext, 1);
			}
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogStrike, Error, TEXT("EnhancedInputComponent requis : vérifiez DefaultInput.ini (DefaultInputComponentClass)."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AStrikePlayerController::OnMove);
	Input->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &AStrikePlayerController::OnLookMouse);
	Input->BindAction(LookGamepadAction, ETriggerEvent::Triggered, this, &AStrikePlayerController::OnLookGamepad);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnJumpStarted);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AStrikePlayerController::OnJumpCompleted);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnCrouchStarted);
	Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AStrikePlayerController::OnCrouchCompleted);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnSprintStarted);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AStrikePlayerController::OnSprintCompleted);
	Input->BindAction(SprintToggleAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnSprintToggle);
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnFireStarted);
	Input->BindAction(FireAction, ETriggerEvent::Completed, this, &AStrikePlayerController::OnFireCompleted);
	Input->BindAction(AimAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnAimStarted);
	Input->BindAction(AimAction, ETriggerEvent::Completed, this, &AStrikePlayerController::OnAimCompleted);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnReload);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnInteractStarted);
	Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &AStrikePlayerController::OnInteractCompleted);
	Input->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnNextWeapon);
	Input->BindAction(PrevWeaponAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnPrevWeapon);
	Input->BindAction(Weapon1Action, ETriggerEvent::Started, this, &AStrikePlayerController::OnWeapon1);
	Input->BindAction(Weapon2Action, ETriggerEvent::Started, this, &AStrikePlayerController::OnWeapon2);
	Input->BindAction(Weapon3Action, ETriggerEvent::Started, this, &AStrikePlayerController::OnWeapon3);
	Input->BindAction(ScoreboardAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnScoreboardStarted);
	Input->BindAction(ScoreboardAction, ETriggerEvent::Completed, this, &AStrikePlayerController::OnScoreboardCompleted);
	Input->BindAction(MenuAction, ETriggerEvent::Started, this, &AStrikePlayerController::OnMenuPressed);
}

// ---------------------------------------------------------------------------
// Gestionnaires
// ---------------------------------------------------------------------------

float AStrikePlayerController::GetAimSensitivityScale() const
{
	const AStrikeCharacter* Character = GetStrikeCharacter();
	if (!Character)
	{
		return 1.f;
	}
	float AimScale = Settings.AimSensitivityMultiplier;
	if (const AStrikeWeapon* Weapon = Character->GetCurrentWeapon())
	{
		AimScale *= FMath::Clamp(Weapon->AimFOV / FMath::Max(Character->BaseFieldOfView, 1.f), 0.1f, 1.f);
	}
	return FMath::Lerp(1.f, AimScale, Character->GetAimAlpha());
}

void AStrikePlayerController::ApplyLook(float YawDegrees, float PitchDegrees)
{
	if (IsLookInputIgnored() || IsMenuMode())
	{
		return;
	}
	FRotator Rotation = GetControlRotation();
	Rotation.Yaw += YawDegrees;
	Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch + PitchDegrees, -89.f, 89.f);
	SetControlRotation(Rotation);
}

void AStrikePlayerController::OnMove(const FInputActionValue& Value)
{
	if (AStrikeCharacter* Character = GetStrikeCharacter())
	{
		Character->MoveInput(Value.Get<FVector2D>());
	}
}

void AStrikePlayerController::OnLookMouse(const FInputActionValue& Value)
{
	// Les axes souris sont déjà pondérés par l'AxisConfig moteur (0.07) : x2.5 = sensation standard UE.
	const FVector2D Axis = Value.Get<FVector2D>();
	const float Scale = 2.5f * Settings.MouseSensitivity * GetAimSensitivityScale();
	ApplyLook(Axis.X * Scale, Axis.Y * Scale * (Settings.bInvertY ? -1.f : 1.f));
}

void AStrikePlayerController::OnLookGamepad(const FInputActionValue& Value)
{
	FVector2D Axis = Value.Get<FVector2D>();
	// Courbe quadratique : précision au centre, vitesse en bout de course.
	Axis = FVector2D(Axis.X * FMath::Abs(Axis.X), Axis.Y * FMath::Abs(Axis.Y));
	const float Rate = 180.f * Settings.GamepadSensitivity * GetAimSensitivityScale() * GetWorld()->GetDeltaSeconds();
	ApplyLook(Axis.X * Rate, Axis.Y * Rate * 0.7f * (Settings.bInvertY ? -1.f : 1.f));
}

void AStrikePlayerController::OnJumpStarted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->StartJump(); } }
void AStrikePlayerController::OnJumpCompleted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->StopJump(); } }
void AStrikePlayerController::OnCrouchStarted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->StartCrouchOrSlide(); } }
void AStrikePlayerController::OnCrouchCompleted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->StopCrouchOrSlide(); } }
void AStrikePlayerController::OnSprintStarted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->SetSprinting(true); } }
void AStrikePlayerController::OnSprintCompleted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->SetSprinting(false); } }

void AStrikePlayerController::OnSprintToggle()
{
	if (AStrikeCharacter* C = GetStrikeCharacter())
	{
		C->SetSprinting(!C->IsSprinting());
	}
}

void AStrikePlayerController::OnFireStarted()
{
	if (IsMenuMode())
	{
		float X = 0.f;
		float Y = 0.f;
		if (GetMousePosition(X, Y))
		{
			if (AStrikeHUD* StrikeHUD = GetHUD<AStrikeHUD>())
			{
				StrikeHUD->HandleMenuClick(FVector2D(X, Y));
			}
		}
		return;
	}
	if (AStrikeCharacter* C = GetStrikeCharacter())
	{
		C->StartFire();
	}
}

void AStrikePlayerController::OnFireCompleted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->StopFire(); } }
void AStrikePlayerController::OnAimStarted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->SetAiming(true); } }
void AStrikePlayerController::OnAimCompleted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->SetAiming(false); } }
void AStrikePlayerController::OnReload() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->ReloadWeapon(); } }

void AStrikePlayerController::OnInteractStarted()
{
	if (AStrikeCharacter* C = GetStrikeCharacter())
	{
		C->Interact();
		C->SetInteractHeld(true);
	}
}

void AStrikePlayerController::OnInteractCompleted() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->SetInteractHeld(false); } }
void AStrikePlayerController::OnNextWeapon() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->CycleWeapon(1); } }
void AStrikePlayerController::OnPrevWeapon() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->CycleWeapon(-1); } }
void AStrikePlayerController::OnWeapon1() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->EquipSlot(0); } }
void AStrikePlayerController::OnWeapon2() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->EquipSlot(1); } }
void AStrikePlayerController::OnWeapon3() { if (AStrikeCharacter* C = GetStrikeCharacter()) { C->EquipSlot(2); } }
void AStrikePlayerController::OnScoreboardStarted() { bScoreboardVisible = true; }
void AStrikePlayerController::OnScoreboardCompleted() { bScoreboardVisible = false; }

void AStrikePlayerController::OnMenuPressed()
{
	if (!IsMenuMode())
	{
		ReturnToMenu();
	}
}

// ---------------------------------------------------------------------------
// Tick : tactile, réglages, résolution adaptative
// ---------------------------------------------------------------------------

void AStrikePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController())
	{
		return;
	}

	RefreshInputMode();

	if (AStrikeCharacter* Character = GetStrikeCharacter())
	{
		Character->BaseFieldOfView = Settings.FieldOfView;
	}

	UpdateTouch(DeltaTime);
	UpdateAdaptiveResolution(DeltaTime);
}

void AStrikePlayerController::UpdateTouch(float DeltaTime)
{
	if (!TouchControls || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);

	TArray<FStrikeTouchEvent> Events;
	TouchControls->Update(this, ViewportSize, Events);

	FVector2D Tap;
	const bool bTapped = TouchControls->ConsumeTap(Tap);
	if (IsMenuMode())
	{
		if (bTapped)
		{
			if (AStrikeHUD* StrikeHUD = GetHUD<AStrikeHUD>())
			{
				StrikeHUD->HandleMenuClick(Tap);
			}
		}
		return;
	}

	for (const FStrikeTouchEvent& Event : Events)
	{
		HandleTouchButton(Event.Id, Event.bPressed);
	}

	AStrikeCharacter* Character = GetStrikeCharacter();
	if (!Character)
	{
		return;
	}

	const FVector2D Move = TouchControls->GetMoveAxis();
	if (!Move.IsNearlyZero())
	{
		Character->MoveInput(Move);
	}

	const bool bWantsSprint = TouchControls->WantsSprint();
	if (bWantsSprint != bTouchSprinting)
	{
		bTouchSprinting = bWantsSprint;
		Character->SetSprinting(bWantsSprint);
	}

	const FVector2D Look = TouchControls->ConsumeLookDelta();
	if (!Look.IsNearlyZero())
	{
		// 0.15°/pixel à 1080p, indépendant de la densité d'écran.
		const float Scale = 0.15f * Settings.TouchSensitivity * (1080.f / FMath::Max(ViewportSize.Y, 1.f)) * GetAimSensitivityScale();
		ApplyLook(Look.X * Scale, -Look.Y * Scale * (Settings.bInvertY ? -1.f : 1.f));
	}
}

void AStrikePlayerController::HandleTouchButton(FName Id, bool bPressed)
{
	AStrikeCharacter* Character = GetStrikeCharacter();

	if (Id == TEXT("Score"))
	{
		if (bPressed)
		{
			bScoreboardVisible = !bScoreboardVisible;
		}
		return;
	}
	if (Id == TEXT("Menu"))
	{
		if (bPressed)
		{
			ReturnToMenu();
		}
		return;
	}
	if (!Character)
	{
		return;
	}

	if (Id == TEXT("Fire") || Id == TEXT("FireLeft"))
	{
		bPressed ? Character->StartFire() : Character->StopFire();
	}
	else if (Id == TEXT("Aim") && bPressed)
	{
		bTouchAimToggled = !Character->IsAiming();
		Character->SetAiming(bTouchAimToggled);
	}
	else if (Id == TEXT("Jump"))
	{
		bPressed ? Character->StartJump() : Character->StopJump();
	}
	else if (Id == TEXT("Crouch") && bPressed)
	{
		Character->ToggleCrouch();
	}
	else if (Id == TEXT("Reload") && bPressed)
	{
		Character->ReloadWeapon();
	}
	else if (Id == TEXT("Interact"))
	{
		if (bPressed)
		{
			Character->Interact();
		}
		Character->SetInteractHeld(bPressed);
	}
	else if (Id == TEXT("Swap") && bPressed)
	{
		Character->CycleWeapon(1);
	}
}

void AStrikePlayerController::UpdateAdaptiveResolution(float DeltaTime)
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	// Solution de secours : si la résolution dynamique moteur n'est pas prise en charge par l'appareil,
	// on ajuste r.ScreenPercentage selon la fréquence d'images mesurée.
	if (!Settings.bDynamicResolution)
	{
		return;
	}
	FpsAccumTime += DeltaTime;
	++FpsAccumFrames;
	if (FpsAccumTime < 2.f)
	{
		return;
	}
	const float AverageFps = FpsAccumFrames / FpsAccumTime;
	FpsAccumTime = 0.f;
	FpsAccumFrames = 0;

	const float Previous = AdaptiveScreenPercentage;
	if (AverageFps < 27.f)
	{
		AdaptiveScreenPercentage = FMath::Max(50.f, AdaptiveScreenPercentage - 10.f);
	}
	else if (AverageFps > 40.f)
	{
		AdaptiveScreenPercentage = FMath::Min(100.f, AdaptiveScreenPercentage + 5.f);
	}
	if (!FMath::IsNearlyEqual(Previous, AdaptiveScreenPercentage))
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
		{
			CVar->Set(AdaptiveScreenPercentage, ECVF_SetByGameSetting);
		}
	}
#endif
}

void AStrikePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void AStrikePlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	bTouchAimToggled = false;
	bTouchSprinting = false;
	if (TouchControls)
	{
		TouchControls->ResetAll();
	}
}

void AStrikePlayerController::NotifyHitConfirmed(bool bHeadshot, bool bKill)
{
	LastHitMarkerTime = GetWorld()->GetTimeSeconds();
	bLastHitHeadshot = bHeadshot;
	bLastHitKill = bKill;
}

// ---------------------------------------------------------------------------
// Réglages, navigation, commandes
// ---------------------------------------------------------------------------

void AStrikePlayerController::UpdateSettings(const FStrikeSettings& NewSettings)
{
	Settings = NewSettings;
	if (UStrikeGameInstance* GI = GetGameInstance<UStrikeGameInstance>())
	{
		GI->SetSettings(Settings);
	}
}

void AStrikePlayerController::StartMode(const FString& ModeAlias, bool bHost)
{
	const FString Options = FString::Printf(TEXT("game=%s%s?Difficulty=%d"), *ModeAlias, bHost ? TEXT("?listen") : TEXT(""), Settings.BotDifficulty);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, Options);
}

void AStrikePlayerController::ReturnToMenu()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, TEXT("game=Menu"));
}

void AStrikePlayerController::ClientOnExtracted_Implementation(const TArray<FStrikeLootItem>& Items)
{
	if (UStrikeGameInstance* GI = GetGameInstance<UStrikeGameInstance>())
	{
		GI->AddToStash(Items);
	}
}

void AStrikePlayerController::ClientRecordZombieWave_Implementation(int32 Wave)
{
	if (UStrikeGameInstance* GI = GetGameInstance<UStrikeGameInstance>())
	{
		GI->RecordZombieWave(Wave);
	}
}

void AStrikePlayerController::StrikeResetStats()
{
	ServerResetStats();
}

void AStrikePlayerController::ServerResetStats_Implementation()
{
	if (AStrikePlayerState* PS = GetPlayerState<AStrikePlayerState>())
	{
		PS->ResetCombatStats();
	}
}

void AStrikePlayerController::StrikeSensitivity(float Mouse, float Touch)
{
	FStrikeSettings NewSettings = Settings;
	NewSettings.MouseSensitivity = FMath::Clamp(Mouse, 0.05f, 10.f);
	NewSettings.TouchSensitivity = FMath::Clamp(Touch, 0.05f, 10.f);
	UpdateSettings(NewSettings);
}

void AStrikePlayerController::StrikeQuality(int32 Level)
{
	FStrikeSettings NewSettings = Settings;
	NewSettings.GraphicsQuality = FMath::Clamp(Level, -1, 3);
	UpdateSettings(NewSettings);
}

void AStrikePlayerController::StrikeTouch(bool bEnable)
{
	FStrikeSettings NewSettings = Settings;
	NewSettings.bForceTouchControls = bEnable;
	UpdateSettings(NewSettings);
}
