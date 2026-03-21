// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MCPTypes.h"

/**
 * Routes incoming MCP tool-call requests to the correct tool implementation.
 *
 * All tools register themselves during module startup. The handler is
 * intentionally decoupled from networking so it can also be used in tests.
 */
class UNREALMCP_API FMCPCommandHandler
{
public:
	FMCPCommandHandler();
	~FMCPCommandHandler() = default;

	/** Register all built-in tools (actors, blueprints, editor). */
	void RegisterBuiltinTools();

	/** Register a custom tool at runtime. Returns false if the name is already taken. */
	bool RegisterTool(const FMCPToolDescriptor& Descriptor, FMCPToolDelegate Delegate);

	/**
	 * Execute a tool by name with the supplied arguments JSON.
	 * Returns a JSON object that is either a "result" or an "error" payload
	 * suitable for embedding in a JSON-RPC 2.0 response.
	 */
	TSharedPtr<FJsonObject> ExecuteTool(const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments);

	/** Return a JSON array of tool-descriptor objects for tools/list responses. */
	TSharedPtr<FJsonObject> ListTools() const;

private:
	struct FRegisteredTool
	{
		FMCPToolDescriptor Descriptor;
		FMCPToolDelegate   Delegate;
	};

	TMap<FString, FRegisteredTool> RegisteredTools;
};
