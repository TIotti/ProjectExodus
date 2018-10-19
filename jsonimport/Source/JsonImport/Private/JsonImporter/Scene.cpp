#include "JsonImportPrivatePCH.h"

#include "JsonImporter.h"

#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Classes/Components/PointLightComponent.h"
#include "Engine/Classes/Components/SpotLightComponent.h"
#include "Engine/Classes/Components/DirectionalLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Classes/Components/StaticMeshComponent.h"
#include "LevelEditorViewport.h"
#include "Factories/TextureFactory.h"
#include "Factories/MaterialFactoryNew.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionConstant.h"

#include "Factories/WorldFactory.h"

#include "RawMesh.h"

#include "DesktopPlatformModule.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "FJsonImportModule"

void JsonImporter::importScene(JsonObjPtr sceneDataObj, bool createWorld){
	const JsonValPtrs *sceneObjects = 0;

	bool editorMode = !createWorld;

	auto sceneName = getString(sceneDataObj, "name");
	auto scenePath = getString(sceneDataObj, "path");
	auto buildIndex = getInt(sceneDataObj, "buildIndex");
	
	if (scenePath.IsEmpty()){
		UE_LOG(JsonLog, Warning, TEXT("Empty scene path. Scene name  %s, scene path %s"), *sceneName, *scenePath);
	}
	else
		UE_LOG(JsonLog, Log, TEXT("Processing scene \"%s\"(%s)"), *sceneName, *scenePath);

	loadArray(sceneDataObj, sceneObjects, TEXT("objects"), TEXT("Scene objects"));
	if (!createWorld){
		ImportWorkData workData(GEditor->GetEditorWorldContext().World(), editorMode);
		loadObjects(sceneObjects, workData);
		return;
	}

	saveSceneObjectsAsWorld(sceneObjects, sceneName, scenePath);
}

bool JsonImporter::saveSceneObjectsAsWorld(const JsonValPtrs * sceneObjects, const FString &sceneName, const FString &scenePath){
	UWorldFactory *factory = NewObject<UWorldFactory>();
	factory->WorldType = EWorldType::Inactive;
	factory->bInformEngineOfWorld = true;
	factory->FeatureLevel = GEditor->DefaultWorldFeatureLevel;

	UWorld *existingWorld = 0;
	FString worldName = sceneName;
	FString worldFileName = scenePath;
	FString packageName;
	FString outWorldName, outPackageName;
	EObjectFlags flags = RF_Public | RF_Standalone;
	UPackage *worldPackage = 0;

	worldPackage = createPackage(worldName, worldFileName, assetRootPath, 
		FString("Level"), &outPackageName, &outWorldName, &existingWorld);

	if (existingWorld){
		UE_LOG(JsonLog, Warning, TEXT("World already exists for %s(%s)"), *sceneName, *scenePath);
		return false;
	}

	UWorld *newWorld = CastChecked<UWorld>(factory->FactoryCreateNew(
		UWorld::StaticClass(), worldPackage, *outWorldName, flags, 0, GWarn));

	if (newWorld){
		ImportWorkData workData(newWorld, false);
		loadObjects(sceneObjects, workData);
	}

	if (worldPackage){
		FAssetRegistryModule::AssetCreated(newWorld);
		worldPackage->SetDirtyFlag(true);
	}
	return true;
}

UWorld* JsonImporter::createWorldForScene(const FString &sceneName, const FString &scenePath){
	UWorld *newWorld = GEditor->NewMap();
	UE_LOG(JsonLog, Log, TEXT("World created: %x"), newWorld);
	UE_LOG(JsonLog, Log, TEXT("World current level: %x"), newWorld->GetCurrentLevel());
	UE_LOG(JsonLog, Log, TEXT("World persistent level: %x"), newWorld->PersistentLevel);
	return newWorld;
}

bool JsonImporter::saveLoadedWorld(UWorld *world, const FString &sceneName, const FString &scenePath){
	UWorldFactory *factory = NewObject<UWorldFactory>();
	factory->WorldType = EWorldType::Inactive;
	factory->bInformEngineOfWorld = true;
	factory->FeatureLevel = GEditor->DefaultWorldFeatureLevel;

	UWorld *existingWorld = 0;
	FString worldName = sceneName;
	FString worldFileName = scenePath;
	FString packageName;
	FString outWorldName, outPackageName;
	EObjectFlags flags = RF_Public | RF_Standalone;
	UPackage *worldPackage = 0;

	worldPackage = createPackage(worldName, worldFileName, assetRootPath, 
		FString("Level"), &outPackageName, &outWorldName, &existingWorld);

	if (existingWorld){
		return false;
	}

	UWorld *newWorld = CastChecked<UWorld>(factory->FactoryCreateNew(
		UWorld::StaticClass(), worldPackage, *outWorldName, flags, 0, GWarn));

	if (newWorld){
		auto actor = newWorld->SpawnActor<APointLight>();
		auto label = FString("Test light actor for scene: ") + sceneName;
		actor->SetActorLabel(*label); 
	}

	if (worldPackage){
		FAssetRegistryModule::AssetCreated(newWorld);
		worldPackage->SetDirtyFlag(true);
	}
	return true;
}



void JsonImporter::importProject(const FString& filename){
	setupAssetPaths(filename);
	auto jsonData = loadJsonFromFile(filename);
	if (!jsonData){
		UE_LOG(JsonLog, Error, TEXT("Json loading failed, aborting. \"%s\""), *filename);
		return;
	}

	auto configPtr = getObject(jsonData, "config");
	const JsonValPtrs *scenes;
	//auto scenesPtr = jsonData->GetArrayField("scenes");
	loadArray(jsonData, scenes, "scenes");
	auto resources = getObject(jsonData, "resources");//jsonData->GetObjectField("resources");

	if (configPtr && scenes && resources){
		UE_LOG(JsonLog, Display, TEXT("Project nodes detected in file \"%s\". Loading as project"), *filename);
		importResources(resources);

		UE_LOG(JsonLog, Display, TEXT("%d scenes found in file \"%s\"."), scenes->Num(), *filename);
		if (scenes->Num() == 1){
			JsonValPtr curSceneData = (*scenes)[0];
			auto curSceneDataObj = curSceneData->AsObject();
			if (!curSceneDataObj){
				UE_LOG(JsonLog, Error, TEXT("Invalid scene data"));
				return;
			}			
			importScene(curSceneDataObj, false);
			return;
		}

		int numScenes = 0;
		FScopedSlowTask sceneProgress(scenes->Num(), LOCTEXT("Importing scenes", "Importing scenes"));
		sceneProgress.MakeDialog();
		for(int i = 0; i < scenes->Num(); i++){
			JsonValPtr curSceneData = (*scenes)[i];
			auto curSceneDataObj = curSceneData->AsObject();
			if (curSceneDataObj){
				importScene(curSceneDataObj, true);
				numScenes++;
				//break;
			}
			sceneProgress.EnterProgressFrame();
		}
		//project
	}
	else{
		UE_LOG(JsonLog, Warning, TEXT("Project nodes not found. Attempting to load as scene"), *filename);
		importResources(jsonData);

		const JsonValPtrs *objects = 0;
		loadArray(jsonData, objects, TEXT("objects"), TEXT("Objects"));

		ImportWorkData workData(GEditor->GetEditorWorldContext().World(), true);
		loadObjects(objects, workData);
	}
}

void JsonImporter::importScene(const FString& filename){
	setupAssetPaths(filename);

	auto jsonData = loadJsonFromFile(filename);
	if (!jsonData){
		UE_LOG(JsonLog, Error, TEXT("Json loading failed, aborting. \"%s\""), *filename);
		return;
	}

	importResources(jsonData);

	const JsonValPtrs *objects = 0;
	loadArray(jsonData, objects, TEXT("objects"), TEXT("Objects"));
	ImportWorkData workData(GEditor->GetEditorWorldContext().World(), true);
	loadObjects(objects, workData);
}
