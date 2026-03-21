// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.

#include "MCPServer.h"
#include "MCPCommandHandler.h"
#include "MCPTypes.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int32  MCP_RECV_BUFFER_SIZE  = 1024 * 1024; // 1 MB
static constexpr int32  MCP_SEND_BUFFER_SIZE  = 1024 * 1024;
static constexpr uint32 MCP_MAX_MSG_BYTES     = 4 * 1024 * 1024; // 4 MB hard cap

// MCP uses the same Content-Length framing as the Language Server Protocol:
//   Content-Length: <N>\r\n\r\n<JSON bytes>
static const FString HEADER_PREFIX = TEXT("Content-Length: ");

// ---------------------------------------------------------------------------
// FMCPClientSession
// ---------------------------------------------------------------------------

FMCPClientSession::FMCPClientSession(FSocket* InSocket, TSharedRef<FMCPCommandHandler> InCommandHandler)
	: Socket(InSocket)
	, CommandHandler(InCommandHandler)
	, Thread(nullptr)
	, bRunning(false)
	, bInitialized(false)
{
	static int32 SessionCounter = 0;
	const FString ThreadName = FString::Printf(TEXT("MCPClientSession-%d"), ++SessionCounter);
	Thread = FRunnableThread::Create(this, *ThreadName, 0, TPri_Normal);
}

FMCPClientSession::~FMCPClientSession()
{
	RequestShutdown();
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	if (Socket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

bool FMCPClientSession::Init()
{
	bRunning = true;
	return true;
}

uint32 FMCPClientSession::Run()
{
	UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] Client connected"));

	while (bRunning)
	{
		FString RawMessage;
		if (!ReadMessage(RawMessage))
		{
			break;
		}

		const FString Response = DispatchMessage(RawMessage);
		if (!WriteMessage(Response))
		{
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[UnrealMCP] Client disconnected"));
	bRunning = false;
	return 0;
}

void FMCPClientSession::Stop()
{
	bRunning = false;
}

void FMCPClientSession::Exit()
{
	bRunning = false;
}

void FMCPClientSession::RequestShutdown()
{
	bRunning = false;
	if (Socket)
	{
		Socket->Close();
	}
}

// ---------------------------------------------------------------------------
// Message framing  (Content-Length: N\r\n\r\n<body>)
// ---------------------------------------------------------------------------

bool FMCPClientSession::ReadMessage(FString& OutMessage)
{
	// --- 1. Read headers until we find an empty line (CRLFCRLF) ---
	TArray<uint8> HeaderBuf;
	HeaderBuf.Reserve(256);

	while (bRunning)
	{
		uint8 Byte = 0;
		int32 BytesRead = 0;
		if (!Socket->Recv(&Byte, 1, BytesRead, ESocketReceiveFlags::None) || BytesRead == 0)
		{
			// No data yet – yield and retry
			FPlatformProcess::Sleep(0.001f);
			continue;
		}

		HeaderBuf.Add(Byte);

		// Check for \r\n\r\n terminator
		const int32 Len = HeaderBuf.Num();
		if (Len >= 4 &&
			HeaderBuf[Len - 4] == '\r' && HeaderBuf[Len - 3] == '\n' &&
			HeaderBuf[Len - 2] == '\r' && HeaderBuf[Len - 1] == '\n')
		{
			break;
		}
	}

	if (!bRunning)
	{
		return false;
	}

	// --- 2. Parse Content-Length ---
	const FString HeaderStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(HeaderBuf.GetData())));
	int32 ContentLength = -1;

	TArray<FString> Lines;
	HeaderStr.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(HEADER_PREFIX))
		{
			ContentLength = FCString::Atoi(*Line.Mid(HEADER_PREFIX.Len()));
			break;
		}
	}

	if (ContentLength <= 0 || ContentLength > static_cast<int32>(MCP_MAX_MSG_BYTES))
	{
		UE_LOG(LogTemp, Warning, TEXT("[UnrealMCP] Invalid Content-Length: %d"), ContentLength);
		return false;
	}

	// --- 3. Read body ---
	TArray<uint8> Body;
	Body.SetNumZeroed(ContentLength + 1); // +1 for null terminator
	int32 TotalRead = 0;

	while (bRunning && TotalRead < ContentLength)
	{
		int32 BytesRead = 0;
		if (Socket->Recv(Body.GetData() + TotalRead, ContentLength - TotalRead, BytesRead) && BytesRead > 0)
		{
			TotalRead += BytesRead;
		}
		else
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}

	if (TotalRead < ContentLength)
	{
		return false;
	}

	OutMessage = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Body.GetData())));
	return true;
}

bool FMCPClientSession::WriteMessage(const FString& Message)
{
	const FTCHARToUTF8 Converter(*Message);
	const int32 BodyBytes = Converter.Length();

	const FString Header = FString::Printf(TEXT("Content-Length: %d\r\n\r\n"), BodyBytes);
	const FTCHARToUTF8 HeaderConverter(*Header);

	// Send header
	int32 Sent = 0;
	if (!Socket->Send(reinterpret_cast<const uint8*>(HeaderConverter.Get()), HeaderConverter.Length(), Sent))
	{
		return false;
	}

	// Send body
	Sent = 0;
	return Socket->Send(reinterpret_cast<const uint8*>(Converter.Get()), BodyBytes, Sent);
}

// ---------------------------------------------------------------------------
// JSON-RPC dispatch
// ---------------------------------------------------------------------------

FString FMCPClientSession::DispatchMessage(const FString& RawMessage)
{
	TSharedPtr<FJsonObject> Request;
	if (!DeserializeJson(RawMessage, Request) || !Request.IsValid())
	{
		TSharedPtr<FJsonObject> Err = MakeMCPError(EMCPErrorCode::ParseError, TEXT("Parse error"));
		Err->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
		FString Out; SerializeJson(Err, Out);
		return Out;
	}

	// Extract id (may be number, string, or null)
	TSharedPtr<FJsonValue> IdField = Request->TryGetField(TEXT("id"));

	const FString Method = Request->GetStringField(TEXT("method"));
	TSharedPtr<FJsonObject> Params;
	Request->TryGetObjectField(TEXT("params"), Params);

	TSharedPtr<FJsonObject> Response;

	if (Method == TEXT("initialize"))
	{
		Response = HandleInitialize(Params);
	}
	else if (Method == TEXT("initialized"))
	{
		// Notification – no response required; return empty string
		return FString();
	}
	else if (Method == TEXT("tools/list"))
	{
		Response = HandleToolsList();
	}
	else if (Method == TEXT("tools/call"))
	{
		Response = HandleToolsCall(Params);
	}
	else if (Method == TEXT("ping"))
	{
		// Simple liveness ping
		TSharedPtr<FJsonObject> PongResult = MakeShared<FJsonObject>();
		PongResult->SetStringField(TEXT("message"), TEXT("pong"));
		Response = MakeMCPResult(PongResult);
	}
	else
	{
		Response = MakeMCPError(EMCPErrorCode::MethodNotFound, FString::Printf(TEXT("Method not found: %s"), *Method));
	}

	if (!Response.IsValid())
	{
		return FString();
	}

	// Attach id
	if (IdField.IsValid())
	{
		Response->SetField(TEXT("id"), IdField);
	}
	else
	{
		Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
	}

	FString Out;
	SerializeJson(Response, Out);
	return Out;
}

// ---------------------------------------------------------------------------
// MCP protocol handlers
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMCPClientSession::HandleInitialize(const TSharedPtr<FJsonObject>& Params)
{
	bInitialized = true;

	TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), TEXT("unreal-mcp"));
	ServerInfo->SetStringField(TEXT("version"), TEXT("1.0.0"));

	TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();

	// Advertise tool support
	TSharedPtr<FJsonObject> ToolsCapability = MakeShared<FJsonObject>();
	ToolsCapability->SetBoolField(TEXT("listChanged"), false);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCapability);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), TEXT("2024-11-05"));
	Result->SetObjectField(TEXT("capabilities"), Capabilities);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	return MakeMCPResult(Result);
}

TSharedPtr<FJsonObject> FMCPClientSession::HandleToolsList()
{
	return MakeMCPResult(CommandHandler->ListTools());
}

TSharedPtr<FJsonObject> FMCPClientSession::HandleToolsCall(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("tools/call requires params"));
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName) || ToolName.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("Missing required field 'name'"));
	}

	TSharedPtr<FJsonObject> Arguments;
	Params->TryGetObjectField(TEXT("arguments"), Arguments);

	TSharedPtr<FJsonObject> ToolResult = CommandHandler->ExecuteTool(ToolName, Arguments);

	// If the tool returned an error object, propagate it; otherwise wrap in MCP
	// content array as per the MCP spec.
	if (ToolResult.IsValid() && ToolResult->HasField(TEXT("error")))
	{
		return ToolResult;
	}

	// Wrap result payload in MCP tool-result content
	FString ResultText;
	SerializeJson(ToolResult, ResultText);

	TSharedPtr<FJsonObject> ContentItem = MakeShared<FJsonObject>();
	ContentItem->SetStringField(TEXT("type"), TEXT("text"));
	ContentItem->SetStringField(TEXT("text"), ResultText);

	TArray<TSharedPtr<FJsonValue>> ContentArray;
	ContentArray.Add(MakeShared<FJsonValueObject>(ContentItem));

	TSharedPtr<FJsonObject> CallResult = MakeShared<FJsonObject>();
	CallResult->SetArrayField(TEXT("content"), ContentArray);
	CallResult->SetBoolField(TEXT("isError"), false);

	return MakeMCPResult(CallResult);
}

// ---------------------------------------------------------------------------
// FMCPServer
// ---------------------------------------------------------------------------

FMCPServer::FMCPServer(TSharedRef<FMCPCommandHandler> InCommandHandler)
	: CommandHandler(InCommandHandler)
	, ListenSocket(nullptr)
	, ListenThread(nullptr)
	, bRunning(false)
	, BoundPort(0)
{
}

FMCPServer::~FMCPServer()
{
	Shutdown();
}

bool FMCPServer::Start(int32 Port)
{
	ISocketSubsystem* SocketSS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSS)
	{
		UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] No socket subsystem available"));
		return false;
	}

	ListenSocket = SocketSS->CreateSocket(NAME_Stream, TEXT("MCPListenSocket"), false);
	if (!ListenSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] Failed to create listen socket"));
		return false;
	}

	ListenSocket->SetReuseAddr(true);
	ListenSocket->SetNonBlocking(true);

	TSharedRef<FInternetAddr> Addr = SocketSS->CreateInternetAddr();
	Addr->SetAnyAddress();
	Addr->SetPort(Port);

	if (!ListenSocket->Bind(*Addr))
	{
		UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] Failed to bind socket on port %d"), Port);
		SocketSS->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return false;
	}

	if (!ListenSocket->Listen(8))
	{
		UE_LOG(LogTemp, Error, TEXT("[UnrealMCP] Failed to listen on socket"));
		SocketSS->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return false;
	}

	// Retrieve the actual bound port (useful when Port == 0 for auto-assign)
	ListenSocket->GetAddress(*Addr);
	BoundPort = Addr->GetPort();
	if (BoundPort == 0)
	{
		BoundPort = Port;
	}

	bRunning = true;
	ListenThread = FRunnableThread::Create(this, TEXT("MCPServerListen"), 0, TPri_Normal);
	return ListenThread != nullptr;
}

void FMCPServer::Shutdown()
{
	bRunning = false;

	if (ListenSocket)
	{
		ListenSocket->Close();
	}

	if (ListenThread)
	{
		ListenThread->WaitForCompletion();
		delete ListenThread;
		ListenThread = nullptr;
	}

	{
		FScopeLock Lock(&SessionsCS);
		Sessions.Empty();
	}

	if (ListenSocket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}
}

void FMCPServer::Stop()
{
	// FRunnable::Stop – called by thread system; delegate to Shutdown
	Shutdown();
}

bool FMCPServer::Init()
{
	return true;
}

uint32 FMCPServer::Run()
{
	while (bRunning)
	{
		bool bHasPendingConnection = false;
		if (ListenSocket->HasPendingConnection(bHasPendingConnection) && bHasPendingConnection)
		{
			FSocket* ClientSocket = ListenSocket->Accept(TEXT("MCPClient"));
			if (ClientSocket)
			{
				ClientSocket->SetNonBlocking(false);
				ClientSocket->SetBufferSizes(MCP_RECV_BUFFER_SIZE, MCP_SEND_BUFFER_SIZE);

				FScopeLock Lock(&SessionsCS);
				Sessions.Add(MakeUnique<FMCPClientSession>(ClientSocket, CommandHandler));
			}
		}

		CleanupSessions();
		FPlatformProcess::Sleep(0.01f);
	}
	return 0;
}

void FMCPServer::Exit()
{
	bRunning = false;
}

void FMCPServer::CleanupSessions()
{
	FScopeLock Lock(&SessionsCS);
	Sessions.RemoveAll([](const TUniquePtr<FMCPClientSession>& S) {
		return S.IsValid() && !S->IsRunning();
	});
}
