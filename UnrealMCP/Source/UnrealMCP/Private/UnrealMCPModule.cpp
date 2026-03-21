// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.

#include "UnrealMCPModule.h"
#include "MCPServer.h"
#include "MCPCommandHandler.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "FUnrealMCPModule"

void FUnrealMCPModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] Module startup"));

	CommandHandler = MakeShared<FMCPCommandHandler>();
	CommandHandler->RegisterBuiltinTools();

	Server = MakeUnique<FMCPServer>(CommandHandler.ToSharedRef());

	const int32 Port = LoadConfigPort();
	if (Server->Start(Port))
	{
		UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] MCP server listening on port %d"), Port);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] Failed to start MCP server on port %d"), Port);
	}
}

void FUnrealMCPModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] Module shutdown"));
	if (Server.IsValid())
	{
		Server->Shutdown();
	}
}

int32 FUnrealMCPModule::LoadConfigPort() const
{
	int32 Port = 3000;
	GConfig->GetInt(
		TEXT("/Script/UnrealMCP.MCPSettings"),
		TEXT("Port"),
		Port,
		GEditorIni
	);
	return Port;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealMCPModule, UnrealMCP)
