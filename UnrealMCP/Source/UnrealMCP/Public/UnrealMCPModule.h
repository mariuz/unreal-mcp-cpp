// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMCPServer;
class FMCPCommandHandler;

/**
 * Editor module that owns the MCP TCP server lifecycle.
 *
 * The server starts automatically when the editor loads and stops when it
 * shuts down. The port can be configured via DefaultUnrealMCP.ini:
 *
 *   [/Script/UnrealMCP.MCPSettings]
 *   Port=3000
 */
class UNREALMCP_API FUnrealMCPModule : public IModuleInterface
{
public:
	static FUnrealMCPModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUnrealMCPModule>("UnrealMCP");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UnrealMCP");
	}

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Returns the running MCP server instance (may be null before startup). */
	FMCPServer* GetServer() const { return Server.Get(); }

	/** Returns the command handler (tool registry). */
	FMCPCommandHandler* GetCommandHandler() const { return CommandHandler.Get(); }

private:
	int32 LoadConfigPort() const;

	TSharedPtr<FMCPCommandHandler> CommandHandler;
	TUniquePtr<FMCPServer>         Server;
};
