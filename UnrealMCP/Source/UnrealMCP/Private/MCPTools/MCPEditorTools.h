// Copyright (c) 2024 Unreal MCP Contributors. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandler.h"

/**
 * Registers editor-state MCP tools:
 *
 *  - get_editor_info       Return current editor / project state
 *  - get_project_info      Return project name, version, module list
 *  - open_level            Open a level by path
 *  - save_current_level    Save the currently active level
 *  - focus_viewport        Move the perspective viewport camera to a location
 *  - take_screenshot       Capture a viewport screenshot to disk
 *  - undo                  Trigger editor Undo
 *  - redo                  Trigger editor Redo
 */
class FMCPEditorTools
{
public:
	static void RegisterTools(FMCPCommandHandler& Handler);

private:
	static TSharedPtr<FJsonObject> GetEditorInfo      (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> GetProjectInfo     (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> OpenLevel          (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> SaveCurrentLevel   (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> FocusViewport      (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> TakeScreenshot     (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> Undo               (const TSharedPtr<FJsonObject>& Args);
	static TSharedPtr<FJsonObject> Redo               (const TSharedPtr<FJsonObject>& Args);
};
