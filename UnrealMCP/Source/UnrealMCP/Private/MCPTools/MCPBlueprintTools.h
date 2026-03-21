// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandler.h"

/**
 * Registers Blueprint-related MCP tools:
 *
 *  - get_blueprint_info        Return metadata / variable / function list
 *  - create_blueprint          Create a new Blueprint asset
 *  - compile_blueprint         Compile (or recompile) a Blueprint
 *  - add_component_to_blueprint  Add a component to a Blueprint's SCS
 *  - set_blueprint_property    Set a Blueprint variable's default value
 *  - get_blueprint_graph_nodes List the nodes in a Blueprint function graph
 *  - add_blueprint_event_node  Add an event node to a Blueprint event graph
 *  - add_blueprint_function    Add a new custom function to a Blueprint
 */
class FMCPBlueprintTools
{
public:
	static void RegisterTools(FMCPCommandHandler& Handler);

private:
	static TSharedPtr<FJsonObject> GetBlueprintInfo         (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> CreateBlueprint          (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> CompileBlueprint         (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> AddComponentToBlueprint  (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> SetBlueprintProperty     (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> GetBlueprintGraphNodes   (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> AddBlueprintEventNode    (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> AddBlueprintFunction     (const TSharedPtr<FJsonObject>& Args);
};
