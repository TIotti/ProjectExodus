#pragma once
#include "CoreMinimal.h"
#include "JsonObjects.h"
#include "Runtime/CoreUObject/Public/UObject/StrongObjectPtr.h"

class AActor;
using IdActorMap = TMap<int, AActor*>;
using IdSet = TSet<int>;
using IdPair = TPair<JsonId, JsonId>;

using AnimClipIdKey = TPair<JsonId, JsonId>;
using AnimControllerIdKey = TPair<JsonId, JsonId>;

using AnimClipPathMap = TMap<AnimClipIdKey, FString>;
using AnimControllerPathMap = TMap<AnimControllerIdKey, FString>;

AnimClipIdKey makeAnimClipKey(JsonId skeletonId, JsonId clipId){
	return AnimClipIdKey(skeletonId, clipId);
}

AnimClipIdKey makeAnimControlKey(JsonId skeletonId, JsonId controllerId){
	return AnimControllerIdKey(skeletonId, controllerId);
}

class USceneComponent;

/*
This structure holds "spawned game object" information.
A game object can be spawned as a component or as an actor.  
(currently they're being spawned as actors in most cases)

This is represented in this structure. If "component" is set, "actor" is irrelevant or nullptr.
*/
class ImportedObject{
public:
	AActor *actor = nullptr;
	USceneComponent *component = nullptr;

	bool isValid() const{
		return actor || component;
	}

	void setActiveInHierarchy(bool active) const;
	void setFolderPath(const FString &folderPath) const;

	void attachTo(AActor *actor, USceneComponent *component) const;
	void attachTo(ImportedObject *parent) const;

	ImportedObject() = default;

	ImportedObject(AActor *actor_)
	:actor(actor_), component(nullptr){}

	ImportedObject(USceneComponent *component_)
	:actor(nullptr), component(component_){}

	ImportedObject(AActor *actor_, USceneComponent *component_)
	:actor(actor_), component(component_){}
};

using ImportedObjectMap = TMap<JsonId, ImportedObject>;
using ImportedObjectArray = TArray<ImportedObject>;

/*
This one exists mostly to deal with the fact htat IDs are unique within SCENE, 
and within each scene they start from zero.

I kinda wonder if I should work towards ensureing ids being globally unique, but then again...
not much point when I can just use scoped dictionaries.
*/
class ImportWorkData{
public:
	IdNameMap objectFolderPaths;

	ImportedObjectMap importedObjects;
	//IdActorMap objectActors;
	TStrongObjectPtr<UWorld> world;
	//UWorld *world;
	bool editorMode;
	bool storeActors;

	TArray<ImportedObject> rootObjects;
	TArray<ImportedObject> childObjects;
	TArray<ImportedObject> allObjects;

	TArray<AnimControllerIdKey> delayedAnimControllers;

	TArray<JsonId> postProcessAnimatorObjects;

	void registerAnimatorForPostProcessing(const JsonGameObject &jsonObj);
	void registerDelayedAnimController(JsonId skelId, JsonId controllerId);

	void registerObject(const ImportedObject &object, AActor *parent);
	/*
	TArray<AActor*> rootActors;
	TArray<AActor*> childActors;
	TArray<AActor*> allActors;

	void registerActor(AActor* actor, AActor *parent);
	*/

	ImportWorkData(UWorld *world_, bool editorMode_, bool storeActors_ = false)
	:world(world_), editorMode(editorMode_), storeActors(storeActors_){
	}

	void clear(){
		objectFolderPaths.Empty();
		//objectActors.Empty();
		importedObjects.Empty();

		delayedAnimControllers.Empty();
	}
};

