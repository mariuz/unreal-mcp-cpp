# unreal-mcp-cpp

**Unreal MCP** is an Unreal Engine 5 editor plugin that exposes engine functionality to AI agents via the [Model Context Protocol (MCP)](https://modelcontextprotocol.io/). The server is written entirely in **C++** – no Python scripting required – giving AI agents deep, performant access to actors, Blueprints, and editor subsystems.

---

## Why C++?

| Concern | Python scripting | This plugin (C++) |
|---|---|---|
| Deep Blueprint graph access | Limited | Full `EdGraph` API |
| Performance at scale | Slow for 10k+ actors | Native iterator speed |
| Fab / Marketplace distribution | Not standard | Standard C++ plugin |
| Engine subsystem hooks | Partial exposure | Direct subsystem access |

---

## Features

### Actor Tools
| Tool | Description |
|---|---|
| `get_actors_in_level` | List all actors (with optional class filter, max_results) |
| `find_actors_by_name` | Find actors whose label contains a search string |
| `get_actor_properties` | Detailed JSON for a single actor |
| `create_actor` | Spawn a new actor at a given location/rotation |
| `modify_actor` | Set location, rotation, scale, visibility |
| `delete_actor` | Remove an actor from the level |
| `select_actors` | Select actors in the editor viewport |

### Blueprint Tools
| Tool | Description |
|---|---|
| `get_blueprint_info` | Variables, graphs, components of a Blueprint |
| `create_blueprint` | Create a new Blueprint asset |
| `compile_blueprint` | Compile / recompile a Blueprint |
| `add_component_to_blueprint` | Add a component to the SCS |
| `set_blueprint_property` | Set a Blueprint variable default value |
| `get_blueprint_graph_nodes` | List nodes in a function/event graph |
| `add_blueprint_event_node` | Add an event node (e.g. BeginPlay) to EventGraph |
| `add_blueprint_function` | Add a new function graph to a Blueprint |

### Editor Tools
| Tool | Description |
|---|---|
| `get_editor_info` | Current level, PIE state, viewport camera |
| `get_project_info` | Project name, engine version, plugin list |
| `open_level` | Open a level by content path |
| `save_current_level` | Save the active level |
| `focus_viewport` | Move the perspective viewport camera |
| `take_screenshot` | Capture a viewport screenshot |
| `undo` | Trigger editor Undo |
| `redo` | Trigger editor Redo |

---

## Installation

### Method 1 – Copy to project Plugins folder (recommended)
```
MyProject/
└── Plugins/
    └── UnrealMCP/          ← copy this folder here
        ├── UnrealMCP.uplugin
        └── Source/
```

Then regenerate project files and build.

### Method 2 – Engine plugins folder
Copy `UnrealMCP/` into `<UnrealEngine>/Engine/Plugins/Editor/`.

---

## Configuration

The port can be overridden in your project's `Config/DefaultUnrealMCP.ini`:

```ini
[/Script/UnrealMCP.MCPSettings]
Port=3000
```

---

## MCP Protocol

The server implements **MCP 2024-11-05** over **TCP with Content-Length framing** (identical to the Language Server Protocol transport):

```
Content-Length: <N>\r\n
\r\n
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{...}}
```

Default port: **3000**  
Bind address: `0.0.0.0` (all interfaces; firewall accordingly)

### Supported JSON-RPC methods

| Method | Description |
|---|---|
| `initialize` | MCP handshake, returns capabilities |
| `initialized` | Client acknowledgement (notification) |
| `tools/list` | Return all registered tools with JSON Schema |
| `tools/call` | Invoke a tool by name with arguments |
| `ping` | Liveness check → `{"message":"pong"}` |

---

## Quick-start with Claude Desktop / cursor

Add to your MCP client configuration:

```json
{
  "mcpServers": {
    "unreal": {
      "command": "nc",
      "args": ["127.0.0.1", "3000"]
    }
  }
}
```

Or use any MCP-compatible client that supports TCP transport.

### Example: list all Static Mesh actors
```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"get_actors_in_level","arguments":{"class_filter":"StaticMeshActor","max_results":100}}}
```

### Example: create a point light
```json
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"create_actor","arguments":{"class":"PointLight","name":"MyLight","location":{"x":0,"y":0,"z":300}}}}
```

---

## Building

Requires **Unreal Engine 5.3+** with the following modules (all included in the engine):

- `Sockets`, `Networking`, `Json`, `JsonUtilities`
- `UnrealEd`, `BlueprintGraph`, `Kismet`, `KismetCompiler`
- `AssetTools`, `AssetRegistry`, `LevelEditor`

Generate Visual Studio / Xcode project files, then build the `Editor` target.

---

## Architecture

```
FUnrealMCPModule          (IModuleInterface – startup/shutdown)
    │
    ├── FMCPCommandHandler  (tool registry & dispatch)
    │       ├── FMCPActorTools::RegisterTools()
    │       ├── FMCPBlueprintTools::RegisterTools()
    │       └── FMCPEditorTools::RegisterTools()
    │
    └── FMCPServer          (TCP listener thread)
            └── FMCPClientSession × N  (one thread per client)
                    └── ReadMessage / DispatchMessage / WriteMessage
```

All tool callbacks that touch editor state execute on the **Game Thread** via `AsyncTask`, ensuring thread safety with Unreal's non-thread-safe editor APIs.

---

## Extending with Custom Tools

```cpp
FMCPToolDescriptor Desc;
Desc.Name = TEXT("my_custom_tool");
Desc.Description = TEXT("Does something amazing");
Desc.Params = {{ TEXT("value"), TEXT("number"), TEXT("Input value"), true }};

FUnrealMCPModule::Get().GetCommandHandler()->RegisterTool(
    Desc,
    FMCPToolDelegate::CreateLambda([](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
    {
        // ... your implementation ...
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("status"), TEXT("ok"));
        return Result;
    })
);
```

---

## License

MIT License – see [LICENSE](LICENSE) for details.
