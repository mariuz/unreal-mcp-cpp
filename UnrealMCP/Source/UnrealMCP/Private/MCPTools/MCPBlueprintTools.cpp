// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.

#include "MCPTools/MCPBlueprintTools.h"
#include "MCPTypes.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Components/ActorComponent.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UBlueprint* FindBlueprintByPath(const FString& AssetPath)
{
	return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPBlueprintTools::RegisterTools(FMCPCommandHandler& Handler)
{
	// get_blueprint_info
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("get_blueprint_info");
		Desc.Description = TEXT("Return metadata, variables, and functions for a Blueprint asset.");
		Desc.Params = {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content browser path, e.g. '/Game/MyBP'"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::GetBlueprintInfo));
	}

	// create_blueprint
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("create_blueprint");
		Desc.Description = TEXT("Create a new Blueprint asset derived from a parent class.");
		Desc.Params = {
			{ TEXT("name"),        TEXT("string"), TEXT("Asset name without extension"), true },
			{ TEXT("package_path"),TEXT("string"), TEXT("Folder path, e.g. '/Game/Blueprints'"), true },
			{ TEXT("parent_class"),TEXT("string"), TEXT("Parent UClass name (default: Actor)"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::CreateBlueprint));
	}

	// compile_blueprint
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("compile_blueprint");
		Desc.Description = TEXT("Compile (or recompile) a Blueprint asset.");
		Desc.Params = {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Content browser path to the Blueprint"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::CompileBlueprint));
	}

	// add_component_to_blueprint
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("add_component_to_blueprint");
		Desc.Description = TEXT("Add a component to a Blueprint's Simple Construction Script.");
		Desc.Params = {
			{ TEXT("asset_path"),     TEXT("string"), TEXT("Blueprint asset path"), true },
			{ TEXT("component_class"),TEXT("string"), TEXT("Component class name (e.g. 'StaticMeshComponent')"), true },
			{ TEXT("component_name"), TEXT("string"), TEXT("Name for the new component"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::AddComponentToBlueprint));
	}

	// set_blueprint_property
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("set_blueprint_property");
		Desc.Description = TEXT("Set the default value of a variable defined in a Blueprint.");
		Desc.Params = {
			{ TEXT("asset_path"),     TEXT("string"), TEXT("Blueprint asset path"), true },
			{ TEXT("property_name"),  TEXT("string"), TEXT("Variable name"), true },
			{ TEXT("property_value"), TEXT("string"), TEXT("New default value as string"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::SetBlueprintProperty));
	}

	// get_blueprint_graph_nodes
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("get_blueprint_graph_nodes");
		Desc.Description = TEXT("Return all nodes in a Blueprint function or event graph.");
		Desc.Params = {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path"), true },
			{ TEXT("graph_name"), TEXT("string"), TEXT("Graph name (default: 'EventGraph')"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::GetBlueprintGraphNodes));
	}

	// add_blueprint_event_node
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("add_blueprint_event_node");
		Desc.Description = TEXT("Add a standard event node (e.g. BeginPlay) to a Blueprint's EventGraph.");
		Desc.Params = {
			{ TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path"), true },
			{ TEXT("event_name"), TEXT("string"), TEXT("Event name, e.g. 'ReceiveBeginPlay'"), true },
			{ TEXT("position_x"), TEXT("number"), TEXT("Node X position in graph"), false },
			{ TEXT("position_y"), TEXT("number"), TEXT("Node Y position in graph"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::AddBlueprintEventNode));
	}

	// add_blueprint_function
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("add_blueprint_function");
		Desc.Description = TEXT("Add a new custom function to a Blueprint.");
		Desc.Params = {
			{ TEXT("asset_path"),    TEXT("string"), TEXT("Blueprint asset path"), true },
			{ TEXT("function_name"), TEXT("string"), TEXT("Name for the new function"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPBlueprintTools::AddBlueprintFunction));
	}
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMCPBlueprintTools::GetBlueprintInfo(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'asset_path' is required"));
	}

	UBlueprint* BP = FindBlueprintByPath(AssetPath);
	if (!BP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("name"),        BP->GetName());
	Result->SetStringField(TEXT("path"),        AssetPath);
	Result->SetStringField(TEXT("parent_class"), BP->ParentClass ? BP->ParentClass->GetName() : TEXT("None"));

	// Variables
	TArray<TSharedPtr<FJsonValue>> VarArray;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"),     Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"),     Var.VarType.PinCategory.ToString());
		VarObj->SetStringField(TEXT("default"),  Var.DefaultValue);
		VarArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}
	Result->SetArrayField(TEXT("variables"), VarArray);

	// Graphs
	TArray<TSharedPtr<FJsonValue>> GraphArray;
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		GraphArray.Add(MakeShared<FJsonValueString>(Graph->GetName()));
	}
	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		GraphArray.Add(MakeShared<FJsonValueString>(Graph->GetName()));
	}
	Result->SetArrayField(TEXT("graphs"), GraphArray);

	// Components (SCS)
	TArray<TSharedPtr<FJsonValue>> CompArray;
	if (BP->SimpleConstructionScript)
	{
		for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->ComponentTemplate)
			{
				TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
				CompObj->SetStringField(TEXT("name"),  Node->GetVariableName().ToString());
				CompObj->SetStringField(TEXT("class"), Node->ComponentTemplate->GetClass()->GetName());
				CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
			}
		}
	}
	Result->SetArrayField(TEXT("components"), CompArray);

	return Result;
}

TSharedPtr<FJsonObject> FMCPBlueprintTools::CreateBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString BPName;
	FString PackagePath;
	if (!Args->TryGetStringField(TEXT("name"), BPName) || BPName.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'name' is required"));
	}
	if (!Args->TryGetStringField(TEXT("package_path"), PackagePath) || PackagePath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'package_path' is required"));
	}

	FString ParentClassName = TEXT("Actor");
	Args->TryGetStringField(TEXT("parent_class"), ParentClassName);

	UClass* ParentClass = FindFirstObject<UClass>(*ParentClassName);
	if (!ParentClass)
	{
		ParentClass = AActor::StaticClass();
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UObject* NewAsset = AssetTools.CreateAsset(BPName, PackagePath, UBlueprint::StaticClass(), Factory);
	UBlueprint* NewBP = Cast<UBlueprint>(NewAsset);
	if (!NewBP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, TEXT("Failed to create Blueprint asset"));
	}

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"),   TEXT("created"));
	Result->SetStringField(TEXT("path"),     FString::Printf(TEXT("%s/%s"), *PackagePath, *BPName));
	Result->SetStringField(TEXT("name"),     BPName);
	Result->SetStringField(TEXT("parent"),   ParentClass->GetName());
	return Result;
}

TSharedPtr<FJsonObject> FMCPBlueprintTools::CompileBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'asset_path' is required"));
	}

	UBlueprint* BP = FindBlueprintByPath(AssetPath);
	if (!BP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	FKismetEditorUtilities::CompileBlueprint(BP);

	const bool bHasErrors = BP->Status == EBlueprintStatus::BS_Error;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), bHasErrors ? TEXT("error") : TEXT("compiled"));
	Result->SetBoolField  (TEXT("success"), !bHasErrors);
	return Result;
}

TSharedPtr<FJsonObject> FMCPBlueprintTools::AddComponentToBlueprint(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, CompClassName, CompName;
	if (!Args->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'asset_path' is required"));
	}
	if (!Args->TryGetStringField(TEXT("component_class"), CompClassName) || CompClassName.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'component_class' is required"));
	}

	UBlueprint* BP = FindBlueprintByPath(AssetPath);
	if (!BP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	UClass* CompClass = FindFirstObject<UClass>(*CompClassName);
	if (!CompClass || !CompClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, FString::Printf(TEXT("Unknown component class: %s"), *CompClassName));
	}

	if (!BP->SimpleConstructionScript)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, TEXT("Blueprint has no SimpleConstructionScript"));
	}

	Args->TryGetStringField(TEXT("component_name"), CompName);
	if (CompName.IsEmpty())
	{
		CompName = CompClassName;
	}

	USCS_Node* NewNode = BP->SimpleConstructionScript->CreateNode(CompClass, *CompName);
	BP->SimpleConstructionScript->AddNode(NewNode);

	FKismetEditorUtilities::CompileBlueprint(BP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"),         TEXT("component_added"));
	Result->SetStringField(TEXT("component_name"), CompName);
	Result->SetStringField(TEXT("component_class"),CompClassName);
	return Result;
}

TSharedPtr<FJsonObject> FMCPBlueprintTools::SetBlueprintProperty(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, PropName, PropValue;
	if (!Args->TryGetStringField(TEXT("asset_path"),     AssetPath) || AssetPath.IsEmpty() ||
	    !Args->TryGetStringField(TEXT("property_name"),  PropName)  || PropName.IsEmpty()  ||
	    !Args->TryGetStringField(TEXT("property_value"), PropValue))
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'asset_path', 'property_name', and 'property_value' are required"));
	}

	UBlueprint* BP = FindBlueprintByPath(AssetPath);
	if (!BP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	for (FBPVariableDescription& Var : BP->NewVariables)
	{
		if (Var.VarName.ToString() == PropName)
		{
			Var.DefaultValue = PropValue;
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);

			TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetStringField(TEXT("status"), TEXT("property_set"));
			Result->SetStringField(TEXT("name"),   PropName);
			Result->SetStringField(TEXT("value"),  PropValue);
			return Result;
		}
	}

	return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Variable '%s' not found in Blueprint"), *PropName));
}

TSharedPtr<FJsonObject> FMCPBlueprintTools::GetBlueprintGraphNodes(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath;
	if (!Args->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'asset_path' is required"));
	}

	FString GraphName = TEXT("EventGraph");
	Args->TryGetStringField(TEXT("graph_name"), GraphName);

	UBlueprint* BP = FindBlueprintByPath(AssetPath);
	if (!BP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		if (Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}
	if (!TargetGraph)
	{
		for (UEdGraph* Graph : BP->FunctionGraphs)
		{
			if (Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!TargetGraph)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Graph '%s' not found"), *GraphName));
	}

	TArray<TSharedPtr<FJsonValue>> NodeArray;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (!Node) continue;
		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("name"),  Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
		NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
		NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
		NodeArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("graph"), GraphName);
	Result->SetArrayField (TEXT("nodes"), NodeArray);
	Result->SetNumberField(TEXT("count"), NodeArray.Num());
	return Result;
}

TSharedPtr<FJsonObject> FMCPBlueprintTools::AddBlueprintEventNode(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, EventName;
	if (!Args->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'asset_path' is required"));
	}
	if (!Args->TryGetStringField(TEXT("event_name"), EventName) || EventName.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'event_name' is required"));
	}

	UBlueprint* BP = FindBlueprintByPath(AssetPath);
	if (!BP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	// Find (or use first) EventGraph
	UEdGraph* EventGraph = nullptr;
	for (UEdGraph* Graph : BP->UbergraphPages)
	{
		EventGraph = Graph;
		break;
	}
	if (!EventGraph)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, TEXT("Blueprint has no EventGraph"));
	}

	double PosX = 0, PosY = 0;
	Args->TryGetNumberField(TEXT("position_x"), PosX);
	Args->TryGetNumberField(TEXT("position_y"), PosY);

	// Find the function to create the event for
	UClass* BPClass = BP->GeneratedClass ? BP->GeneratedClass : BP->ParentClass;
	UFunction* EventFunc = BPClass ? BPClass->FindFunctionByName(*EventName) : nullptr;
	if (!EventFunc && BP->ParentClass)
	{
		EventFunc = BP->ParentClass->FindFunctionByName(*EventName);
	}

	UK2Node_Event* EventNode = nullptr;
	if (EventFunc)
	{
		// Check if an override event node already exists for this function
		for (UEdGraphNode* Node : EventGraph->Nodes)
		{
			UK2Node_Event* ExistingEvent = Cast<UK2Node_Event>(Node);
			if (ExistingEvent && ExistingEvent->EventReference.GetMemberName() == *EventName)
			{
				EventNode = ExistingEvent;
				break;
			}
		}
	}

	if (!EventNode)
	{
		// Create event node directly
		EventNode = NewObject<UK2Node_Event>(EventGraph);
		EventNode->EventReference.SetExternalMember(*EventName, BP->ParentClass ? BP->ParentClass : AActor::StaticClass());
		EventNode->bOverrideFunction = (EventFunc != nullptr);
		EventNode->NodePosX = static_cast<int32>(PosX);
		EventNode->NodePosY = static_cast<int32>(PosY);
		EventGraph->AddNode(EventNode, false, false);
		EventNode->PostPlacedNewNode();
		EventNode->AllocateDefaultPins();
	}

	FKismetEditorUtilities::CompileBlueprint(BP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"),     TEXT("node_added"));
	Result->SetStringField(TEXT("event_name"), EventName);
	return Result;
}

TSharedPtr<FJsonObject> FMCPBlueprintTools::AddBlueprintFunction(const TSharedPtr<FJsonObject>& Args)
{
	FString AssetPath, FuncName;
	if (!Args->TryGetStringField(TEXT("asset_path"),    AssetPath) || AssetPath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'asset_path' is required"));
	}
	if (!Args->TryGetStringField(TEXT("function_name"), FuncName) || FuncName.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'function_name' is required"));
	}

	UBlueprint* BP = FindBlueprintByPath(AssetPath);
	if (!BP)
	{
		return MakeMCPError(EMCPErrorCode::ToolError, FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		BP, *FuncName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

	FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, NewGraph, false, nullptr);

	FKismetEditorUtilities::CompileBlueprint(BP);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"),        TEXT("function_added"));
	Result->SetStringField(TEXT("function_name"), FuncName);
	return Result;
}
