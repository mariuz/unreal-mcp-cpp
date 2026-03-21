// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MCPTypes.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

class FMCPCommandHandler;

/**
 * Handles a single client connection: reads Content-Length-framed JSON-RPC
 * messages, dispatches them to FMCPCommandHandler, and writes the response.
 *
 * Runs on a dedicated thread per connection.
 */
class FMCPClientSession : public FRunnable
{
public:
	FMCPClientSession(FSocket* InSocket, TSharedRef<FMCPCommandHandler> InCommandHandler);
	virtual ~FMCPClientSession() override;

	// FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/** Request orderly shutdown. */
	void RequestShutdown();

	bool IsRunning() const { return bRunning; }

private:
	/** Read exactly one framed message. Returns false on error/disconnect. */
	bool ReadMessage(FString& OutMessage);

	/** Write a framed response string to the socket. */
	bool WriteMessage(const FString& Message);

	/** Dispatch a JSON-RPC message and produce a serialised response string. */
	FString DispatchMessage(const FString& RawMessage);

	/** Handle MCP 'initialize' handshake. */
	TSharedPtr<FJsonObject> HandleInitialize(const TSharedPtr<FJsonObject>& Params);

	/** Handle MCP 'tools/list'. */
	TSharedPtr<FJsonObject> HandleToolsList();

	/** Handle MCP 'tools/call'. */
	TSharedPtr<FJsonObject> HandleToolsCall(const TSharedPtr<FJsonObject>& Params);

	FSocket*                           Socket;
	TSharedRef<FMCPCommandHandler>     CommandHandler;
	FRunnableThread*                   Thread;
	TAtomic<bool>                      bRunning;
	bool                               bInitialized;
};

/**
 * TCP server that listens on a configurable port and spawns one
 * FMCPClientSession per accepted connection.
 *
 * The listener itself runs on a background thread; sessions each get their
 * own thread so long-running tool calls don't block other clients.
 */
class UNREALMCP_API FMCPServer : public FRunnable
{
public:
	explicit FMCPServer(TSharedRef<FMCPCommandHandler> InCommandHandler);
	virtual ~FMCPServer() override;

	/**
	 * Start listening.
	 * @param Port  TCP port to bind (default 3000).
	 * @return true if the socket was successfully bound.
	 */
	bool Start(int32 Port = 3000);

	/** Stop listening and close all active sessions. */
	void Shutdown();

	bool IsRunning() const { return bRunning; }
	int32 GetPort() const { return BoundPort; }

	// FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	void CleanupSessions();

	TSharedRef<FMCPCommandHandler>        CommandHandler;
	FSocket*                              ListenSocket;
	FRunnableThread*                      ListenThread;
	TArray<TUniquePtr<FMCPClientSession>> Sessions;
	FCriticalSection                      SessionsCS;
	TAtomic<bool>                         bRunning;
	int32                                 BoundPort;
};
