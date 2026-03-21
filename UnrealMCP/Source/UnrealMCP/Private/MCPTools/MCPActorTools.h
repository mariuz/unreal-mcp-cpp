// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandler.h"

/**
 * Registers actor-related MCP tools:
 *
 *  - get_actors_in_level   List all actors (with optional class filter)
 *  - find_actors_by_name   Find actors whose label matches a pattern
 *  - get_actor_properties  Return detailed property JSON for one actor
 *  - create_actor          Spawn a new actor of a given class
 *  - modify_actor          Set location / rotation / scale / properties
 *  - delete_actor          Remove an actor from the level
 *  - select_actors         Select actors in the editor viewport
 */
class FMCPActorTools
{
public:
	/** Register all actor tools with the supplied command handler. */
	static void RegisterTools(FMCPCommandHandler& Handler);

private:
	static TSharedPtr<FJsonObject> GetActorsInLevel    (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> FindActorsByName    (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> GetActorProperties  (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> CreateActor         (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> ModifyActor         (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> DeleteActor         (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> SelectActors        (const TSharedPtr<FJsonObject>& Args);

	/** Serialise a single actor's metadata to JSON. */
	static TSharedPtr<FJsonObject> ActorToJson(AActor* Actor);
};
