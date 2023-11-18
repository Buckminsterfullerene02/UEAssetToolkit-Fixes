#include "Toolkit/AssetTypeGenerator/AnimSequenceGenerator.h"
#include "AutomatedAssetImportData.h"
#include "Dom/JsonObject.h"
#include "Toolkit/ObjectHierarchySerializer.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxImportUI.h"
#include "Factories/ReimportFbxAnimSequenceFactory.h"

void UAnimSequenceGenerator::CreateAssetPackage() {
	UPackage* NewPackage = CreatePackage(
#if ENGINE_MINOR_VERSION < 26
	nullptr, 
#endif
*GetPackageName().ToString());
	UAnimSequence* NewAnimSequence = ImportAnimation(NewPackage, GetAssetName(), RF_Public | RF_Standalone);
	if (NewAnimSequence == NULL) return;
	SetPackageAndAsset(NewPackage, NewAnimSequence);
	//PopulateAnimationProperties(NewAnimSequence);
}

void UAnimSequenceGenerator::OnExistingPackageLoaded() {
	UAnimSequence* ExistingAnimation = GetAsset<UAnimSequence>();
	
	if (!IsAnimationSourceUpToDate(ExistingAnimation)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing AnimationSequence %s Source File"), *GetPackageName().ToString());
		//ReimportAnimationFromSource(ExistingAnimation);
	}
	
	if (!IsAnimationPropertiesUpToDate(ExistingAnimation)) {
		UE_LOG(LogAssetGenerator, Log, TEXT("Refreshing AnimationSequence %s Properties"), *GetPackageName().ToString());
		//PopulateAnimationProperties(ExistingAnimation);
	}
}

UAnimSequence* UAnimSequenceGenerator::ImportAnimation(UPackage* Package, const FName& AssetName, const EObjectFlags ObjectFlags) {
	UFbxFactory* AnimationFactory = NewObject<UFbxFactory>(GetTransientPackage(), NAME_None);
	
	AnimationFactory->SetAutomatedAssetImportData(NewObject<UAutomatedAssetImportData>(AnimationFactory));
	AnimationFactory->SetDetectImportTypeOnImport(false);
	const bool LowFrameWarning = SetupFbxImportSettings(AnimationFactory->ImportUI);

	const FString AssetFbxFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("fbx"));
	
	bool bOperationCancelled = false;
	UObject* ResultAnimation = AnimationFactory->ImportObject(UAnimSequence::StaticClass(), Package, AssetName, ObjectFlags, AssetFbxFilePath, TEXT(""), bOperationCancelled);
	if (LowFrameWarning && ResultAnimation == NULL) {
		UE_LOG(LogAssetGenerator, Warning, TEXT("Anim %s not imported due to having only one or two frames, skipping!"), *GetPackageName().ToString());
		return NULL;
	}
	
	checkf(ResultAnimation, TEXT("Failed to import AnimSequence %s from FBX file %s. See log for errors"), *GetPackageName().ToString(), *AssetFbxFilePath);
	checkf(ResultAnimation->GetOuter() == Package, TEXT("Expected Outer to be package %s, found %s"), *Package->GetName(), *ResultAnimation->GetOuter()->GetPathName());
	checkf(ResultAnimation->GetFName() == AssetName, TEXT("Expected Name to be %s, but found %s"), *AssetName.ToString(), *ResultAnimation->GetName());
	
	return CastChecked<UAnimSequence>(ResultAnimation);
}

void UAnimSequenceGenerator::ReimportAnimationFromSource(UAnimSequence* Asset) {
	UClass* ReimportFbxAnimSequenceFactoryClass = FindObjectChecked<UClass>(NULL, TEXT("/Script/UnrealEd.ReimportFbxAnimSequenceFactory"));
	UReimportFbxAnimSequenceFactory* AnimationFactory = static_cast<UReimportFbxAnimSequenceFactory*>(NewObject<UObject>(GetTransientPackage(), ReimportFbxAnimSequenceFactoryClass));
	
	AnimationFactory->SetAutomatedAssetImportData(NewObject<UAutomatedAssetImportData>(AnimationFactory));
	AnimationFactory->SetDetectImportTypeOnImport(false);
	if (SetupFbxImportSettings(AnimationFactory->ImportUI))
	{
		UE_LOG(LogAssetGenerator, Warning, TEXT("Anim %s not imported due to having only one or two frames, skipping!"), *GetPackageName().ToString());
		return;
	}
	
	const FString AssetFbxFilePath = GetAdditionalDumpFilePath(TEXT(""), TEXT("fbx"));
	
	AnimationFactory->SetReimportPaths(Asset, {AssetFbxFilePath});
	AnimationFactory->Reimport(Asset);
	MarkAssetChanged();
}

bool UAnimSequenceGenerator::SetupFbxImportSettings(UFbxImportUI* ImportUI) const {
	ImportUI->MeshTypeToImport = FBXIT_Animation;
	ImportUI->bOverrideFullName = true;
	ImportUI->bImportMaterials = false;
	ImportUI->bImportTextures = false;
	ImportUI->bImportAnimations = true;
	ImportUI->OverrideAnimationName = GetAssetName().ToString();

	const int32 SkeletonObjectIndex = GetAssetData()->GetObjectField(TEXT("AssetObjectData"))->GetIntegerField(TEXT("Skeleton"));
	USkeleton* Skeleton = Cast<USkeleton>(GetObjectSerializer()->DeserializeObject(SkeletonObjectIndex));
	if (Skeleton) {
		ImportUI->Skeleton = Skeleton;	
	} else {
		UE_LOG(LogAssetGenerator, Error, TEXT("Failed to find skeleton for animation %s"), *GetPackageName().ToString());
		return true;
	}

	ImportUI->AnimSequenceImportData = NewObject<UFbxAnimSequenceImportData>(ImportUI, NAME_None, RF_NoFlags);
	
	int32 NumFrames = GetAssetData()->GetObjectField(TEXT("AssetObjectData"))->GetIntegerField(TEXT("NumFrames"));
	ImportUI->AnimSequenceImportData->AnimationLength = FBXALIT_SetRange;
	
	ImportUI->AnimSequenceImportData->bRemoveRedundantKeys = true;
	ImportUI->AnimSequenceImportData->bUseDefaultSampleRate = false;
	
	ImportUI->AnimSequenceImportData->FrameImportRange = FInt32Interval(0, NumFrames - 1);
	UE_LOG(LogAssetGenerator, Log, TEXT("NumFrames: %d"), NumFrames);

	if (NumFrames <= 2)
	{
		UE_LOG(LogAssetGenerator, Warning, TEXT("Animation %s has low frame count!"), *GetPackageName().ToString());
		ImportUI->AnimSequenceImportData->FrameImportRange = FInt32Interval(0, NumFrames);
	}

	double RateScale = GetAssetData()->GetObjectField(TEXT("AssetObjectData"))->GetNumberField(TEXT("RateScale"));
	if (RateScale == 0) RateScale = 1;

	// Limit the frame range to 3k otherwise we'll never import the full animation...
	// TODO: Make this an option if required?
	const int32 MAX_FRAMES = 3000;
	if (NumFrames > MAX_FRAMES) {
		UE_LOG(LogAssetGenerator, Warning, TEXT("Limiting num frames of %s to %d"), *GetPackageName().ToString(), MAX_FRAMES);
		NumFrames = MAX_FRAMES;
		ImportUI->AnimSequenceImportData->CustomSampleRate = 60 * RateScale;
	} else {
		const double SequenceLength = GetAssetData()->GetObjectField(TEXT("AssetObjectData"))->GetNumberField(TEXT("SequenceLength"));
		const int32 CustomSampleRate = FMath::CeilToInt((float)NumFrames / (float)SequenceLength) * RateScale;
		ImportUI->AnimSequenceImportData->CustomSampleRate = CustomSampleRate;
		
		UE_LOG(LogAssetGenerator, Log, TEXT("SequenceLength: %f"), SequenceLength);
		UE_LOG(LogAssetGenerator, Log, TEXT("RateScale: %f"), RateScale);
		UE_LOG(LogAssetGenerator, Log, TEXT("CustomSampleRate: %d"), CustomSampleRate);
	}

	return NumFrames <= 2;
}

void UAnimSequenceGenerator::PopulateAnimationProperties(UAnimSequence* Asset) {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	GetObjectSerializer()->DeserializeObjectProperties(AssetObjectProperties.ToSharedRef(), Asset);

	int MaxAnimNotifyTrack = 0;

	for (const FAnimNotifyEvent& Notify : Asset->Notifies) {
		MaxAnimNotifyTrack = FMath::Max(MaxAnimNotifyTrack, Notify.TrackIndex);
	}

	if (MaxAnimNotifyTrack > Asset->AnimNotifyTracks.Num() - 1) {
		Asset->AnimNotifyTracks.SetNum(MaxAnimNotifyTrack + 1);
	}
	
	MarkAssetChanged();
}

bool UAnimSequenceGenerator::IsAnimationPropertiesUpToDate(UAnimSequence* Asset) const {
	const TSharedPtr<FJsonObject> AssetData = GetAssetData();
	const TSharedPtr<FJsonObject> AssetObjectProperties = AssetData->GetObjectField(TEXT("AssetObjectData"));

	if(!GetObjectSerializer()->AreObjectPropertiesUpToDate(AssetObjectProperties.ToSharedRef(), Asset)) {
		return false;
	}

	int MaxAnimNotifyTrack = 0;

	for (const FAnimNotifyEvent& Notify : Asset->Notifies) {
		MaxAnimNotifyTrack = FMath::Max(MaxAnimNotifyTrack, Notify.TrackIndex);
	}

	if (MaxAnimNotifyTrack > Asset->AnimNotifyTracks.Num() - 1) {
		return false;
	}

	return true;
}

bool UAnimSequenceGenerator::IsAnimationSourceUpToDate(UAnimSequence* Asset) const {
	//TEMPFIX: Old dump files do not have the correct model file hash inside of them, so assume asset is up to date when it is missing
	if (!GetAssetData()->HasField(TEXT("ModelFileHash"))) {
		return true;
	}
	
	const FAssetImportInfo& AssetImportInfo = Asset->AssetImportData->SourceData;
	const FMD5Hash& ExistingFileHash = AssetImportInfo.SourceFiles[0].FileHash;
	
	const FString ExistingFileHashString = LexToString(ExistingFileHash);
	const FString ModelFileHash = GetAssetData()->GetStringField(TEXT("ModelFileHash"));
	return ExistingFileHashString == ModelFileHash;
}

void UAnimSequenceGenerator::PopulateStageDependencies(TArray<FPackageDependency>& OutDependencies) const {
	if (GetCurrentStage() == EAssetGenerationStage::CONSTRUCTION) {
		PopulateReferencedObjectsDependencies(OutDependencies);
	}
}

FName UAnimSequenceGenerator::GetAssetClass() {
	return UAnimSequence::StaticClass()->GetFName();
}
