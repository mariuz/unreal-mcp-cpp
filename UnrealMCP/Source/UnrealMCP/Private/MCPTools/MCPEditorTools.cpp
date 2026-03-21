// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.

#include "MCPTools/MCPEditorTools.h"
#include "MCPTypes.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "LevelEditor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IProjectManager.h"
#include "FileHelpers.h"
#include "HighResScreenshot.h"

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void FMCPEditorTools::RegisterTools(FMCPCommandHandler& Handler)
{
	// get_editor_info
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("get_editor_info");
		Desc.Description = TEXT("Return information about the current editor state: active level, PIE status, viewport camera.");
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::GetEditorInfo));
	}

	// get_project_info
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("get_project_info");
		Desc.Description = TEXT("Return project name, version, engine version, and loaded modules.");
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::GetProjectInfo));
	}

	// open_level
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("open_level");
		Desc.Description = TEXT("Open a level by its content-browser path (e.g. '/Game/Maps/MyLevel').");
		Desc.Params = {
			{ TEXT("level_path"), TEXT("string"), TEXT("Content browser path to the level asset"), true },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::OpenLevel));
	}

	// save_current_level
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("save_current_level");
		Desc.Description = TEXT("Save the currently active level to disk.");
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::SaveCurrentLevel));
	}

	// focus_viewport
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("focus_viewport");
		Desc.Description = TEXT("Move the perspective editor viewport camera to a world location.");
		Desc.Params = {
			{ TEXT("x"), TEXT("number"), TEXT("World X"), false },
			{ TEXT("y"), TEXT("number"), TEXT("World Y"), false },
			{ TEXT("z"), TEXT("number"), TEXT("World Z"), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::FocusViewport));
	}

	// take_screenshot
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("take_screenshot");
		Desc.Description = TEXT("Capture a viewport screenshot. Returns the output file path.");
		Desc.Params = {
			{ TEXT("filename"), TEXT("string"), TEXT("Output filename (without extension). Saved to project Saved/Screenshots."), false },
		};
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::TakeScreenshot));
	}

	// undo
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("undo");
		Desc.Description = TEXT("Trigger editor Undo (Ctrl+Z).");
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::Undo));
	}

	// redo
	{
		FMCPToolDescriptor Desc;
		Desc.Name = TEXT("redo");
		Desc.Description = TEXT("Trigger editor Redo (Ctrl+Y).");
		Handler.RegisterTool(Desc, FMCPToolDelegate::CreateStatic(&FMCPEditorTools::Redo));
	}
}

// ---------------------------------------------------------------------------
// Implementations
// ---------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMCPEditorTools::GetEditorInfo(const TSharedPtr<FJsonObject>& /*Args*/)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!GEditor)
	{
		Result->SetStringField(TEXT("status"), TEXT("no_editor"));
		return Result;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		Result->SetStringField(TEXT("current_level"), World->GetMapName());
		Result->SetBoolField  (TEXT("is_pie_active"),  World->IsPlayInEditor());
	}

	// Viewport camera
	for (FEditorViewportClient* Client : GEditor->GetAllViewportClients())
	{
		if (Client && Client->IsPerspective())
		{
			const FVector  CamLoc = Client->GetViewLocation();
			const FRotator CamRot = Client->GetViewRotation();

			TSharedPtr<FJsonObject> CamObj = MakeShared<FJsonObject>();
			CamObj->SetNumberField(TEXT("x"), CamLoc.X);
			CamObj->SetNumberField(TEXT("y"), CamLoc.Y);
			CamObj->SetNumberField(TEXT("z"), CamLoc.Z);
			Result->SetObjectField(TEXT("camera_location"), CamObj);

			TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
			RotObj->SetNumberField(TEXT("pitch"), CamRot.Pitch);
			RotObj->SetNumberField(TEXT("yaw"),   CamRot.Yaw);
			RotObj->SetNumberField(TEXT("roll"),  CamRot.Roll);
			Result->SetObjectField(TEXT("camera_rotation"), RotObj);
			break;
		}
	}

	Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	return Result;
}

TSharedPtr<FJsonObject> FMCPEditorTools::GetProjectInfo(const TSharedPtr<FJsonObject>& /*Args*/)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("project_name"),   FApp::GetProjectName());
	Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());

	const FProjectDescriptor* Desc = IProjectManager::Get().GetCurrentProject();
	if (Desc)
	{
		Result->SetStringField(TEXT("engine_association"), Desc->EngineAssociation);

		// Plugin list
		TArray<TSharedPtr<FJsonValue>> PluginArray;
		for (const FPluginReferenceDescriptor& Plugin : Desc->Plugins)
		{
			TSharedPtr<FJsonObject> PlugObj = MakeShared<FJsonObject>();
			PlugObj->SetStringField(TEXT("name"),    Plugin.Name);
			PlugObj->SetBoolField  (TEXT("enabled"), Plugin.bEnabled);
			PluginArray.Add(MakeShared<FJsonValueObject>(PlugObj));
		}
		Result->SetArrayField(TEXT("plugins"), PluginArray);
	}

	return Result;
}

TSharedPtr<FJsonObject> FMCPEditorTools::OpenLevel(const TSharedPtr<FJsonObject>& Args)
{
	FString LevelPath;
	if (!Args->TryGetStringField(TEXT("level_path"), LevelPath) || LevelPath.IsEmpty())
	{
		return MakeMCPError(EMCPErrorCode::InvalidParams, TEXT("'level_path' is required"));
	}

	if (!GEditor)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("GEditor not available"));
	}

	// FEditorFileUtils::LoadMap loads and sets the level as current
	const bool bResult = FEditorFileUtils::LoadMap(LevelPath, false, true);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"),     bResult ? TEXT("opened") : TEXT("failed"));
	Result->SetStringField(TEXT("level_path"), LevelPath);
	Result->SetBoolField  (TEXT("success"),    bResult);
	return Result;
}

TSharedPtr<FJsonObject> FMCPEditorTools::SaveCurrentLevel(const TSharedPtr<FJsonObject>& /*Args*/)
{
	if (!GEditor)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("GEditor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("No editor world"));
	}

	const bool bSaved = FEditorFileUtils::SaveCurrentLevel();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"),  bSaved ? TEXT("saved") : TEXT("failed"));
	Result->SetBoolField  (TEXT("success"), bSaved);
	Result->SetStringField(TEXT("level"),   World->GetMapName());
	return Result;
}

TSharedPtr<FJsonObject> FMCPEditorTools::FocusViewport(const TSharedPtr<FJsonObject>& Args)
{
	if (!GEditor)
	{
		return MakeMCPError(EMCPErrorCode::InternalError, TEXT("GEditor not available"));
	}

	FVector Target = FVector::ZeroVector;
	Args->TryGetNumberField(TEXT("x"), Target.X);
	Args->TryGetNumberField(TEXT("y"), Target.Y);
	Args->TryGetNumberField(TEXT("z"), Target.Z);

	for (FEditorViewportClient* Client : GEditor->GetAllViewportClients())
	{
		if (Client && Client->IsPerspective())
		{
			Client->SetViewLocation(Target);
			Client->Invalidate();
			break;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("focused"));
	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Target.X);
	LocObj->SetNumberField(TEXT("y"), Target.Y);
	LocObj->SetNumberField(TEXT("z"), Target.Z);
	Result->SetObjectField(TEXT("location"), LocObj);
	return Result;
}

TSharedPtr<FJsonObject> FMCPEditorTools::TakeScreenshot(const TSharedPtr<FJsonObject>& Args)
{
	FString Filename = TEXT("mcp_screenshot");
	Args->TryGetStringField(TEXT("filename"), Filename);

	const FString SaveDir  = FPaths::ProjectSavedDir() / TEXT("Screenshots");
	const FString FilePath = SaveDir / Filename + TEXT(".png");

	// Request a high-res screenshot via the global request system
	FScreenshotRequest::RequestScreenshot(FilePath, false, false);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"),   TEXT("requested"));
	Result->SetStringField(TEXT("filepath"), FilePath);
	return Result;
}

TSharedPtr<FJsonObject> FMCPEditorTools::Undo(const TSharedPtr<FJsonObject>& /*Args*/)
{
	if (GEditor)
	{
		GEditor->UndoTransaction();
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("undo_executed"));
	return Result;
}

TSharedPtr<FJsonObject> FMCPEditorTools::Redo(const TSharedPtr<FJsonObject>& /*Args*/)
{
	if (GEditor)
	{
		GEditor->RedoTransaction();
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("redo_executed"));
	return Result;
}
