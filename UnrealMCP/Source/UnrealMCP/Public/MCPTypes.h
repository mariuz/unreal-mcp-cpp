// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// ---------------------------------------------------------------------------
// MCP / JSON-RPC 2.0 types
// ---------------------------------------------------------------------------

/** JSON-RPC 2.0 error codes as defined by the specification. */
namespace EMCPErrorCode
{
	enum Type : int32
	{
		ParseError     = -32700,
		InvalidRequest = -32600,
		MethodNotFound = -32601,
		InvalidParams  = -32602,
		InternalError  = -32603,
		// MCP-specific application errors start at -32000
		ToolNotFound   = -32000,
		ToolError      = -32001,
	};
}

/** A single MCP tool parameter descriptor. */
struct FMCPToolParam
{
	FString Name;
	FString Type;        // "string" | "number" | "boolean" | "object" | "array"
	FString Description;
	bool    bRequired = false;
};

/** Descriptor for a single MCP tool. */
struct FMCPToolDescriptor
{
	FString                  Name;
	FString                  Description;
	TArray<FMCPToolParam>    Params;
};

/** Delegate signature for tool implementation functions. */
DECLARE_DELEGATE_RetVal_OneParam(
	TSharedPtr<FJsonObject> /*Result or ErrorObject*/,
	FMCPToolDelegate,
	const TSharedPtr<FJsonObject>& /*Arguments*/
);

/** Helper: build a standard JSON-RPC 2.0 success response. */
static FORCEINLINE TSharedPtr<FJsonObject> MakeMCPResult(TSharedPtr<FJsonObject> ResultPayload)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetObjectField(TEXT("result"), ResultPayload.IsValid() ? ResultPayload : MakeShared<FJsonObject>());
	return Response;
}

/** Helper: build a standard JSON-RPC 2.0 error response. */
static FORCEINLINE TSharedPtr<FJsonObject> MakeMCPError(int32 Code, const FString& Message, TSharedPtr<FJsonValue> Data = nullptr)
{
	TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetNumberField(TEXT("code"), Code);
	ErrorObj->SetStringField(TEXT("message"), Message);
	if (Data.IsValid())
	{
		ErrorObj->SetField(TEXT("data"), Data);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetObjectField(TEXT("error"), ErrorObj);
	return Response;
}

/** Helper: serialise a JsonObject to a UTF-8 string. */
static FORCEINLINE bool SerializeJson(const TSharedPtr<FJsonObject>& JsonObject, FString& OutString)
{
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
	return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
}

/** Helper: deserialise a UTF-8 string to a JsonObject. */
static FORCEINLINE bool DeserializeJson(const FString& InString, TSharedPtr<FJsonObject>& OutObject)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InString);
	return FJsonSerializer::Deserialize(Reader, OutObject);
}
