// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.

#include "MCPCommandHandler.h"
#include "MCPTools/MCPActorTools.h"
#include "MCPTools/MCPBlueprintTools.h"
#include "MCPTools/MCPEditorTools.h"
#include "Async/Async.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FMCPCommandHandler::FMCPCommandHandler() = default;

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

void FMCPCommandHandler::RegisterBuiltinTools()
{
	FMCPActorTools::RegisterTools(*this);
	FMCPBlueprintTools::RegisterTools(*this);
	FMCPEditorTools::RegisterTools(*this);
}

bool FMCPCommandHandler::RegisterTool(const FMCPToolDescriptor& Descriptor, FMCPToolDelegate Delegate)
{
	if (RegisteredTools.Contains(Descriptor.Name))
	{
		UE_LOG(LogTemp, Warning, TEXT("[UnrealMCP] Tool '%s' is already registered"), *Descriptor.Name);
		return false;
	}
	RegisteredTools.Add(Descriptor.Name, FRegisteredTool{ Descriptor, Delegate });
	UE_LOG(LogTemp, Verbose, TEXT("[UnrealMCP] Registered tool: %s"), *Descriptor.Name);
	return true;
}

// ---------------------------------------------------------------------------
// Tool execution
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMCPCommandHandler::ExecuteTool(const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments)
{
	const FRegisteredTool* Tool = RegisteredTools.Find(ToolName);
	if (!Tool)
	{
		return MakeMCPError(EMCPErrorCode::ToolNotFound, FString::Printf(TEXT("Unknown tool: %s"), *ToolName));
	}

	if (!Tool->Delegate.IsBound())
	{
		return MakeMCPError(EMCPErrorCode::InternalError, FString::Printf(TEXT("Tool '%s' has no delegate"), *ToolName));
	}

	TSharedPtr<FJsonObject> Args = Arguments.IsValid() ? Arguments : MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> ToolResult;

	// All tool calls that touch the editor must run on the game thread.
	if (IsInGameThread())
	{
		ToolResult = Tool->Delegate.Execute(Args);
	}
	else
	{
		TSharedPtr<FJsonObject> ResultHolder;
		TAtomic<bool> bDone(false);

		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			ResultHolder = Tool->Delegate.Execute(Args);
			bDone = true;
		});

		// Spin-wait – MCP client sessions run on worker threads and the game
		// thread must be ticking for this to complete.
		while (!bDone)
		{
			FPlatformProcess::Sleep(0.001f);
		}
		ToolResult = ResultHolder;
	}

	return ToolResult.IsValid() ? ToolResult : MakeMCPError(EMCPErrorCode::InternalError, TEXT("Tool returned null"));
}

// ---------------------------------------------------------------------------
// tools/list
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMCPCommandHandler::ListTools() const
{
	TArray<TSharedPtr<FJsonValue>> ToolArray;

	for (const auto& Pair : RegisteredTools)
	{
		const FMCPToolDescriptor& Desc = Pair.Value.Descriptor;

		TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
		ToolObj->SetStringField(TEXT("name"), Desc.Name);
		ToolObj->SetStringField(TEXT("description"), Desc.Description);

		// Input schema (JSON Schema subset)
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> RequiredArray;

		for (const FMCPToolParam& Param : Desc.Params)
		{
			TSharedPtr<FJsonObject> ParamSchema = MakeShared<FJsonObject>();
			ParamSchema->SetStringField(TEXT("type"), Param.Type);
			ParamSchema->SetStringField(TEXT("description"), Param.Description);
			PropertiesObj->SetObjectField(Param.Name, ParamSchema);

			if (Param.bRequired)
			{
				RequiredArray.Add(MakeShared<FJsonValueString>(Param.Name));
			}
		}

		InputSchema->SetObjectField(TEXT("properties"), PropertiesObj);
		if (RequiredArray.Num() > 0)
		{
			InputSchema->SetArrayField(TEXT("required"), RequiredArray);
		}

		ToolObj->SetObjectField(TEXT("inputSchema"), InputSchema);
		ToolArray.Add(MakeShared<FJsonValueObject>(ToolObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), ToolArray);
	return Result;
}
