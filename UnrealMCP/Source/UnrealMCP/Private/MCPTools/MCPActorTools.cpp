// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.

#include "MCPTools/MCPActorTools.h"
#include "MCPTypes.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Selection.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "Math/Transform.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UWorld* GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

static AActor* FindActorByLabel(UWorld* World, const FString& Label)
{
	if (!World) return nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == Label)
		{
			return *It;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Serialise a single actor
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMCPActorTools::ActorToJson(AActor* Actor)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Actor) return Obj;

	Obj->SetStringField(TEXT("name"),  Actor->GetActorLabel());
	Obj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Obj->SetStringField(TEXT("path"),  Actor->GetPathName());
	Obj->SetBoolField  (TEXT("hidden"), Actor->IsHidden());
	Obj->SetBoolField  (TEXT("selected"), Actor->IsSelected());

	const FVector  Loc   = Actor->GetActorLocation();
	const FRotator Rot   = Actor->GetActorRotation();
	const FVector  Scale = Actor->GetActorScale3D();

	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Loc.X);
	LocObj->SetNumberField(TEXT("y"), Loc.Y);
	LocObj->SetNumberField(TEXT("z"), Loc.Z);
	Obj->SetObjectField(TEXT("location"), LocObj);

	TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
	RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
	RotObj->SetNumberField(TEXT("yaw"),   Rot.Yaw);
	RotObj->SetNumberField(TEXT("roll"),  Rot.Roll);
	Obj->SetObjectField(TEXT("rotation"), RotObj);

	TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
	ScaleObj->SetNumberField(TEXT("x"), Scale.X);
	ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
	ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
	Obj->SetObjectField(TEXT("scale"), ScaleObj);

	// Components
	TArray<TSharedPtr<FJsonValue>> CompArray;
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (Comp)
		{
			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"),  Comp->GetName());
			CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
			CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}
	Obj->SetArrayField(TEXT("components"), CompArray);

	return Obj;
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

void FMCPActorTools::RegisterTools(FMCPCommandHandler& Handler)
{
	// get_actors_in_level
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("get_actors_in_level");
		Desc.Description = TEXT("Return a list of all actors in the current editor level. Optionally filter by class name.");
		Desc.Params = {
			{ TEXT("class_filter"), TEXT("string"), TEXT("Optional UClass name to filter by (e.g. 'StaticMeshActor')"), false },
			{ TEXT("max_results"),  TEXT("number"), TEXT("Maximum number of actors to return (default 500)"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPActorTools::GetActorsInLevel));
	}

	// find_actors_by_name
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("find_actors_by_name");
		Desc.Description = TEXT("Find actors whose label contains the search string (case-insensitive).");
		Desc.Params = {
			{ TEXT("search"), TEXT("string"), TEXT("Substring to search for in actor labels"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPActorTools::FindActorsByName));
	}

	// get_actor_properties
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("get_actor_properties");
		Desc.Description = TEXT("Get detailed properties for a single actor identified by its label.");
		Desc.Params = {
			{ TEXT("name"), TEXT("string"), TEXT("Exact actor label"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPActorTools::GetActorProperties));
	}

	// create_actor
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("create_actor");
		Desc.Description = TEXT("Spawn a new actor of the specified class at the given location.");
		Desc.Params = {
			{ TEXT("class"),    TEXT("string"), TEXT("UClass path or short name (e.g. 'StaticMeshActor', 'PointLight')"), true },
			{ TEXT("name"),     TEXT("string"), TEXT("Label for the new actor"), false },
			{ TEXT("location"), TEXT("object"), TEXT("{ x, y, z } in world units"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("{ pitch, yaw, roll } in degrees"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPActorTools::CreateActor));
	}

	// modify_actor
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("modify_actor");
		Desc.Description = TEXT("Modify an actor's transform or visibility by label.");
		Desc.Params = {
			{ TEXT("name"),     TEXT("string"), TEXT("Exact actor label"), true },
			{ TEXT("location"), TEXT("object"), TEXT("New { x, y, z } world location"), false },
			{ TEXT("rotation"), TEXT("object"), TEXT("New { pitch, yaw, roll } rotation"), false },
			{ TEXT("scale"),    TEXT("object"), TEXT("New { x, y, z } scale"), false },
			{ TEXT("hidden"),   TEXT("boolean"),TEXT("Show/hide the actor"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPActorTools::ModifyActor));
	}

	// delete_actor
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("delete_actor");
		Desc.Description = TEXT("Delete an actor from the level by label.");
		Desc.Params = {
			{ TEXT("name"), TEXT("string"), TEXT("Exact actor label to delete"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPActorTools::DeleteActor));
	}

	// select_actors
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("select_actors");
		Desc.Description = TEXT("Select one or more actors in the editor viewport by their labels.");
		Desc.Params = {
			{ TEXT("names"),      TEXT("array"),   TEXT("Array of actor labels to select"), true },
			{ TEXT("add_to_selection"), TEXT("boolean"), TEXT("If true, add to the current selection instead of replacing"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPActorTools::SelectActors));
	}
}

// ---------------------------------------------------------------------------
// Tool implementations
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMCPActorTools::GetActorsInLevel(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world available"));
	}

	FString ClassFilter;
	Args->TryGetStringField(TEXT("class_filter"), ClassFilter);

	int32 MaxResults = 500;
	double MaxResultsDouble = 0;
	if (Args->TryGetNumberField(TEXT("max_results"), MaxResultsDouble) && MaxResultsDouble > 0)
	{
		MaxResults = static_cast<int32>(MaxResultsDouble);
	}

	UClass* FilterClass = nullptr;
	if (!ClassFilter.IsEmpty())
	{
		FilterClass = FindFirstObject<UClass>(*ClassFilter);
	}

	TArray<TSharedPtr<FJsonValue>> ActorArray;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (ActorArray.Num() >= MaxResults) break;
		if (FilterClass && !It->IsA(FilterClass)) continue;

		ActorArray.Add(MakeShared<FJsonValueObject>(ActorToJson(*It)));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("actors"), ActorArray);
	Result->SetNumberField(TEXT("count"), ActorArray.Num());
	return Result;
}

TSharedPtr<FJsonObject> FMCPActorTools::FindActorsByName(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world available"));
	}

	FString Search;
	if (!Args->TryGetStringField(TEXT("search"), Search) || Search.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'search' parameter is required"));
	}

	TArray<TSharedPtr<FJsonValue>> ActorArray;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel().Contains(Search, ESearchCase::IgnoreCase))
		{
			ActorArray.Add(MakeShared<FJsonValueObject>(ActorToJson(*It)));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("actors"), ActorArray);
	Result->SetNumberField(TEXT("count"), ActorArray.Num());
	return Result;
}

TSharedPtr<FJsonObject> FMCPActorTools::GetActorProperties(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world available"));
	}

	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'name' parameter is required"));
	}

	AActor* Actor = FindActorByLabel(World, Name);
	if (!Actor)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	return ActorToJson(Actor);
}

TSharedPtr<FJsonObject> FMCPActorTools::CreateActor(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world available"));
	}

	FString ClassName;
	if (!Args->TryGetStringField(TEXT("class"), ClassName) || ClassName.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'class' parameter is required"));
	}

	UClass* ActorClass = FindFirstObject<UClass>(*ClassName);
	if (!ActorClass)
	{
		// Try common class mappings
		static const TMap<FString, FString> ClassAliases = {
			{ TEXT("StaticMesh"),   TEXT("StaticMeshActor") },
			{ TEXT("PointLight"),   TEXT("PointLight") },
			{ TEXT("SpotLight"),    TEXT("SpotLight") },
			{ TEXT("DirectionalLight"), TEXT("DirectionalLight") },
		};
		const FString* Alias = ClassAliases.Find(ClassName);
		if (Alias)
		{
			ActorClass = FindFirstObject<UClass>(**Alias);
		}
	}

	if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, FString::Printf(TEXT("Unknown actor class: %s"), *ClassName));
	}

	FVector Location = FVector::ZeroVector;
	const TSharedPtr<FJsonObject>* LocObj = nullptr;
	if (Args->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
	{
		(*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
		(*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
		(*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
	}

	FRotator Rotation = FRotator::ZeroRotator;
	const TSharedPtr<FJsonObject>* RotObj = nullptr;
	if (Args->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj)
	{
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"),   Rotation.Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"),  Rotation.Roll);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (!NewActor)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, TEXT("Failed to spawn actor"));
	}

	FString ActorName;
	if (Args->TryGetStringField(TEXT("name"), ActorName) && !ActorName.IsEmpty())
	{
		NewActor->SetActorLabel(ActorName);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("created"));
	Result->SetObjectField(TEXT("actor"), ActorToJson(NewActor));
	return Result;
}

TSharedPtr<FJsonObject> FMCPActorTools::ModifyActor(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world available"));
	}

	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'name' parameter is required"));
	}

	AActor* Actor = FindActorByLabel(World, Name);
	if (!Actor)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	// Location
	const TSharedPtr<FJsonObject>* LocObj = nullptr;
	if (Args->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
	{
		FVector Loc = Actor->GetActorLocation();
		(*LocObj)->TryGetNumberField(TEXT("x"), Loc.X);
		(*LocObj)->TryGetNumberField(TEXT("y"), Loc.Y);
		(*LocObj)->TryGetNumberField(TEXT("z"), Loc.Z);
		Actor->SetActorLocation(Loc);
	}

	// Rotation
	const TSharedPtr<FJsonObject>* RotObj = nullptr;
	if (Args->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj)
	{
		FRotator Rot = Actor->GetActorRotation();
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"),   Rot.Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"),  Rot.Roll);
		Actor->SetActorRotation(Rot);
	}

	// Scale
	const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
	if (Args->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj)
	{
		FVector Scale = Actor->GetActorScale3D();
		(*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
		(*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
		(*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
		Actor->SetActorScale3D(Scale);
	}

	// Hidden
	bool bHidden = false;
	if (Args->TryGetBoolField(TEXT("hidden"), bHidden))
	{
		Actor->SetIsTemporarilyHiddenInEditor(bHidden);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("modified"));
	Result->SetObjectField(TEXT("actor"), ActorToJson(Actor));
	return Result;
}

TSharedPtr<FJsonObject> FMCPActorTools::DeleteActor(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world available"));
	}

	FString Name;
	if (!Args->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'name' parameter is required"));
	}

	AActor* Actor = FindActorByLabel(World, Name);
	if (!Actor)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Actor '%s' not found"), *Name));
	}

	const FString ActorPath = Actor->GetPathName();
	World->DestroyActor(Actor);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("deleted"));
	Result->SetStringField(TEXT("path"),   ActorPath);
	return Result;
}

TSharedPtr<FJsonObject> FMCPActorTools::SelectActors(const TSharedPtr<FJsonObject>& Args)
{
	UWorld* World = GetEditorWorld();
	if (!World || !GEditor)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world available"));
	}

	const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
	if (!Args->TryGetArrayField(TEXT("names"), NamesArray) || !NamesArray)
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'names' parameter is required"));
	}

	bool bAdd = false;
	Args->TryGetBoolField(TEXT("add_to_selection"), bAdd);

	if (!bAdd)
	{
		GEditor->SelectNone(true, true);
	}

	int32 Selected = 0;
	for (const TSharedPtr<FJsonValue>& Val : *NamesArray)
	{
		const FString Label = Val->AsString();
		AActor* Actor = FindActorByLabel(World, Label);
		if (Actor)
		{
			GEditor->SelectActor(Actor, true, false);
			++Selected;
		}
	}

	GEditor->NoteSelectionChange();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("selected_count"), Selected);
	Result->SetStringField(TEXT("status"), TEXT("ok"));
	return Result;
}
