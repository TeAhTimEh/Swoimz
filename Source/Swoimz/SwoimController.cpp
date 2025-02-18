// Fill out your copyright notice in the Description page of Project Settings.

#include "Swoimz.h"
#include "SwoimController.h"
#include "Swoim.h"
#include "EngineUtils.h"

#define print(text) if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5, FColor::White,text)


// Sets default values
ASwoimController::ASwoimController()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


	// create the box for spawn volume
	WhereToSpawn = CreateDefaultSubobject<UBoxComponent>(TEXT("WhereToSpawn"));
	RootComponent = WhereToSpawn;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->AttachTo(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->AddLocalOffset(FVector(-1, 0, 2));
	CameraBoom->bUsePawnControlRotation = false; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->AttachTo(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	

	// Defaults
	Speedlimit = 400;
	Forcelimit = 40;
	SepFactor = 10;
	AliFactor = 5;
	CohFactor = 5;
	CenFactor = 20;
	AtkFactor = 300;
	AvoFactor1 = 30000000;
	AvoFactor2 = 10;
	SepDistance = 100;
	AliDistance = 1500;
	CohDistance = 15000;
	LookAheadDistance = 30;
	LookAheadDecay = 1.5;
	attackRadius = 30000000;
	CameraOptionSwitch = false;
	NumberOfSwoimers = 50;
}

// Called when the game starts or when spawned
void ASwoimController::BeginPlay()
{
	Super::BeginPlay();
	
	SwoimersArray;
	for (int i = 0; i < NumberOfSwoimers; i++){
		SwoimersArray.Add(SpawnSwoimer());
	}
	print("swoimers spawned");
	for (int i = 0; i < NumberOfSwoimers; i++){
		SwoimersArray[i]->SwoimersArray = SwoimersArray;
		SwoimersArray[i]->center = WhereToSpawn->Bounds.Origin;
		SwoimersArray[i]->Speedlimit = Speedlimit;
		SwoimersArray[i]->Forcelimit = Forcelimit;
		SwoimersArray[i]->SepFactor = SepFactor;
		SwoimersArray[i]->AliFactor = AliFactor;
		SwoimersArray[i]->CohFactor = CohFactor;
		SwoimersArray[i]->CenFactor = CenFactor;
		SwoimersArray[i]->AtkFactor = AtkFactor;
		SwoimersArray[i]->AvoFactor1 = AvoFactor1;
		SwoimersArray[i]->AvoFactor2 = AvoFactor2;
		SwoimersArray[i]->SepDistance = SepDistance;
		SwoimersArray[i]->AliDistance = AliDistance;
		SwoimersArray[i]->CohDistance = CohDistance;
		SwoimersArray[i]->LookAheadDistance = LookAheadDistance;
		SwoimersArray[i]->LookAheadDecay = LookAheadDecay;		
		SwoimersArray[i]->SwoimController = TWeakObjectPtr<ASwoimController>(this);

	}
	UE_LOG(LogTemp, Warning, TEXT("swoimers updated"));
	center = WhereToSpawn->Bounds.Origin;

	

	
}

// Called every frame
void ASwoimController::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	FVector mouseLocation, mouseDirection;
	UWorld* const World = GetWorld();
	//UE_LOG(LogTemp, Warning, TEXT("swoim is controlled by %s"), *Controller->GetStateName().ToString());
	if (Controller->IsLocalPlayerController()){
		//UE_LOG(LogTemp, Warning, TEXT("testing swoim %s"), *(center).ToString());
		//UE_LOG(LogTemp, Warning, TEXT("swoim is controlled by %s"), *Controller->GetStateName().ToString());
		//UE_LOG(LogTemp, Warning, TEXT("swoim is controlled"));
		APlayerController* playerController = (APlayerController*)Controller;// = World->GetFirstPlayerController();
		playerController->DeprojectMousePositionToWorld(mouseLocation, mouseDirection);
		FVector CameraLocation;
		FRotator CameraDirection;
		playerController->GetPlayerViewPoint(CameraLocation, CameraDirection);

		if (!mouseLocation.ContainsNaN()) {
			float t = CameraLocation.Z / (CameraLocation - mouseLocation).Z;
			center = (mouseLocation - CameraLocation) * t + CameraLocation;
		}

	}
	FVector swoimCM = FVector(0, 0, 0);
	
	for (auto& other : SwoimersArray)
	{
		if (other->IsValidLowLevel()){
			swoimCM += other->GetActorLocation();
		}
	}
	swoimCM = swoimCM / (SwoimersArray.Num() - 1);

	swoimCM.Z = 300;

	float alpha = 0.8;

	UE_LOG(LogTemp, Warning, TEXT("swoimers center %s"),*swoimCM.ToString());
	
	if(CameraOptionSwitch)
		SetActorLocation(swoimCM * (1-alpha) + GetActorLocation() * alpha);
	else
		SetActorLocation(center);


}
/*
FVector ASwoimController::GetRandomPointInVolume()
{
	FVector SpawnOrigin = WhereToSpawn->Bounds.Origin;
	FVector SpawnExtent = WhereToSpawn->Bounds.BoxExtent;

	return UKismetMathLibrary::RandomPointInBoundingBox(SpawnOrigin, SpawnExtent);

}*/

// Called to bind functionality to input
void ASwoimController::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);

	// Attack and Disengage
	InputComponent->BindAction("Attack", IE_Pressed, this, &ASwoimController::AttackSwoim);
	InputComponent->BindAction("Disengage", IE_Released, this, &ASwoimController::Disengage);

	// Disperse and return
	InputComponent->BindAction("Disperse", IE_Pressed, this, &ASwoimController::Disperse);
	InputComponent->BindAction("ReturnToFlock", IE_Released, this, &ASwoimController::ReturnToFlock);

	// Camera Zoom
	InputComponent->BindAxis("Zoom", this, &ASwoimController::ZoomCamera);

	// Increase Coh Factor
	InputComponent->BindAction("Increase Coh Factor", IE_Repeat, this, &ASwoimController::IncreaseCohFactor);

	// Camera Center
	InputComponent->BindAction("Camera Center", IE_Pressed, this, &ASwoimController::CameraCenter);

	// Decrease Coh Factor
	InputComponent->BindAction("Decrease Coh Factor", IE_Repeat, this, &ASwoimController::DecreaseCohFactor);

	// Increase Sep Factor
	InputComponent->BindAction("Increase Sep Factor", IE_Repeat, this, &ASwoimController::IncreaseSepFactor);

	// Decrease Sep Factor
	InputComponent->BindAction("Decrease Sep Factor", IE_Repeat, this, &ASwoimController::DecreaseSepFactor);

	// Increase Ali Factor
	InputComponent->BindAction("Increase Ali Factor", IE_Repeat, this, &ASwoimController::IncreaseAliFactor);

	// Decrease Ali Factor
	InputComponent->BindAction("Decrease Ali Factor", IE_Repeat, this, &ASwoimController::DecreaseAliFactor);

	// Increase Coh Distance
	InputComponent->BindAction("Increase Coh Distance", IE_Repeat, this, &ASwoimController::IncreaseCohDistance);

	// Decrease Coh Distance
	InputComponent->BindAction("Decrease Coh Distance", IE_Repeat, this, &ASwoimController::DecreaseCohDistance);

	// Increase Sep Distance
	InputComponent->BindAction("Increase Sep Distance", IE_Repeat, this, &ASwoimController::IncreaseSepDistance);

	// Decrease Sep Distance
	InputComponent->BindAction("Decrease Sep Distance", IE_Repeat, this, &ASwoimController::DecreaseSepDistance);

	// Increase Ali Distance
	InputComponent->BindAction("Increase Ali Distance", IE_Repeat, this, &ASwoimController::IncreaseAliDistance);

	// Decrease Ali Distance
	InputComponent->BindAction("Decrease Ali Distance", IE_Repeat, this, &ASwoimController::DecreaseAliDistance);


}


ASwoim* ASwoimController::SpawnSwoimer()
{
	if (WhatToSpawn != NULL)
	{
		UWorld* const World = GetWorld();
		if (World)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = Instigator;

			FVector SpawnLocation = WhereToSpawn->Bounds.Origin;

			FRotator SpawnRotation;
			SpawnRotation.Yaw = FMath::FRand() * 360.0f;
			SpawnRotation.Pitch = FMath::FRand() * 360.0f;
			SpawnRotation.Roll = FMath::FRand() * 360.0f;

			return World->SpawnActor<ASwoim>(WhatToSpawn, SpawnLocation, SpawnRotation, SpawnParams);
		}
	}
	return NULL;
}

void ASwoimController::AttackSwoim() {
	UE_LOG(LogTemp, Warning, TEXT("swoimers Attacking"));
	TArray<ASwoim*> targetSwoimers;	
	UE_LOG(LogTemp, Warning, TEXT("swoimers Attacking %d other swoimz"), targetSwoimers.Num());
	for (TActorIterator<ASwoimController>itr(GetWorld()); itr; ++itr) {		
		if (!itr) continue;
		if (!itr->IsValidLowLevel()) continue;
		float distanceToSwoim = (itr->center - center).Size();
		if (distanceToSwoim >= 0) {
			UE_LOG(LogTemp, Warning, TEXT("testing swoim %s"), *(itr->center).ToString());
			UE_LOG(LogTemp, Warning, TEXT("testing swoim %s"), *(itr->GetName()));
			UE_LOG(LogTemp, Warning, TEXT("testing swoim at %f"), distanceToSwoim);
		}
		if (distanceToSwoim > 0 && distanceToSwoim < attackRadius) {
			targetSwoimers.Append(itr->SwoimersArray);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("swoimers Attacking %d other swoimz"), targetSwoimers.Num());
	if (targetSwoimers.Num() > 0) {
		for (auto& other : SwoimersArray) {

			int32 indexToAttack = FMath::RandRange(0, targetSwoimers.Num() - 1);
			//UE_LOG(LogTemp, Warning, TEXT("swoimers Attacking swoimer at %d"), indexToAttack);
			other->targetSwoimer = targetSwoimers[indexToAttack];
		}
	}
}

void ASwoimController::Disengage() {
	UE_LOG(LogTemp, Warning, TEXT("swoimers Disengaging"));
	for (auto& other : SwoimersArray) {		
		other->targetSwoimer = NULL;
	}
}

void ASwoimController::Disperse() {
	UE_LOG(LogTemp, Warning, TEXT("swoimers Disperseing"));
	for (auto& swoimer : SwoimersArray) {
		swoimer->CohFactor = swoimer->CohFactor * -10;
		swoimer->CenFactor = 0;
	}
}

void ASwoimController::ReturnToFlock() {
	UE_LOG(LogTemp, Warning, TEXT("swoimers returning"));
	for (auto& swoimer : SwoimersArray) {
		swoimer->CohFactor = swoimer->CohFactor * -0.1;
		swoimer->CenFactor = CenFactor;
	}
}


void ASwoimController::ZoomCamera(float AxisValue) {

	CameraBoom->TargetArmLength = CameraBoom->TargetArmLength + AxisValue;
}

void ASwoimController::IncreaseCohFactor() {
	UE_LOG(LogTemp, Warning, TEXT("CohFactor %f"), CohFactor);
	CohFactor += 0.5;
	for (auto& swoimer : SwoimersArray) {
		swoimer->CohFactor = CohFactor;		
	}
}
void ASwoimController::CameraCenter() {	
	UE_LOG(LogTemp, Warning, TEXT("changing center"));
	if(CameraOptionSwitch)
		CameraOptionSwitch = false;
	else	
		CameraOptionSwitch = true;
}
void ASwoimController::DecreaseCohFactor() {
	UE_LOG(LogTemp, Warning, TEXT("CohFactor %f"), CohFactor);
	CohFactor -= 0.5;
	for (auto& swoimer : SwoimersArray) {
		swoimer->CohFactor = CohFactor;
	}
}

void ASwoimController::IncreaseSepFactor() {
	UE_LOG(LogTemp, Warning, TEXT("SepFactor %f"), SepFactor);
	SepFactor += 0.5;
	for (auto& swoimer : SwoimersArray) {
		swoimer->SepFactor = SepFactor;
	}
}
void ASwoimController::DecreaseSepFactor() {
	UE_LOG(LogTemp, Warning, TEXT("SepFactor %f"), SepFactor);
	SepFactor -= 0.5;
	for (auto& swoimer : SwoimersArray) {
		swoimer->SepFactor = SepFactor;
	}
}

void ASwoimController::IncreaseAliFactor() {
	UE_LOG(LogTemp, Warning, TEXT("AliFactor %f"), AliFactor);
	AliFactor += 0.5;
	for (auto& swoimer : SwoimersArray) {
		swoimer->AliFactor = AliFactor;
	}
}
void ASwoimController::DecreaseAliFactor() {
	UE_LOG(LogTemp, Warning, TEXT("AliFactor %f"), AliFactor);
	AliFactor -= 0.5;
	for (auto& swoimer : SwoimersArray) {
		swoimer->AliFactor = AliFactor;
	}
}

void ASwoimController::IncreaseCohDistance() {
	UE_LOG(LogTemp, Warning, TEXT("CohDistance %f"), CohDistance);
	CohDistance += 10;
	for (auto& swoimer : SwoimersArray) {
		swoimer->CohDistance = CohDistance;
	}
}
void ASwoimController::DecreaseCohDistance() {
	UE_LOG(LogTemp, Warning, TEXT("CohDistance %f"), CohDistance);
	CohDistance -= 10;
	for (auto& swoimer : SwoimersArray) {
		swoimer->CohDistance = CohDistance;
	}
}

void ASwoimController::IncreaseSepDistance() {
	UE_LOG(LogTemp, Warning, TEXT("SepDistance %f"), SepDistance);
	SepDistance += 10;
	for (auto& swoimer : SwoimersArray) {
		swoimer->SepDistance = SepDistance;
	}
}
void ASwoimController::DecreaseSepDistance() {
	UE_LOG(LogTemp, Warning, TEXT("SepDistance %f"), SepDistance);
	SepDistance -= 10;
	for (auto& swoimer : SwoimersArray) {
		swoimer->SepDistance = SepDistance;
	}
}

void ASwoimController::IncreaseAliDistance() {
	UE_LOG(LogTemp, Warning, TEXT("AliDistance %f"), AliDistance);
	AliDistance += 10;
	for (auto& swoimer : SwoimersArray) {
		swoimer->AliDistance = AliDistance;
	}
}
void ASwoimController::DecreaseAliDistance() {
	UE_LOG(LogTemp, Warning, TEXT("AliDistance %f"), AliDistance);
	AliDistance -= 10;
	for (auto& swoimer : SwoimersArray) {
		swoimer->AliDistance = AliDistance;
	}
}