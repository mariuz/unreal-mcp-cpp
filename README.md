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

Then:

1. Right-click your `.uproject` and regenerate project files (or use **Tools → Refresh Visual Studio Project** in the editor).
2. Open the project in Visual Studio/Xcode and build the `Editor` target (for example `MyProjectEditor` in `Development Editor`).
3. Launch the Unreal Editor, open **Edit → Plugins**, and verify **Unreal MCP** is enabled.
4. Restart the editor if prompted.

### Method 2 – Engine plugins folder
Copy `UnrealMCP/` into `<UnrealEngine>/Engine/Plugins/Editor/`.

Then:

1. Regenerate engine/project files.
2. Build your project `Editor` target (or build Unreal Editor from source if you use a source-built engine).
3. Open Unreal Editor and confirm **Unreal MCP** is enabled in **Edit → Plugins**.
4. Restart the editor if prompted.

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

## Connecting from an MCP Client

The UnrealMCP server listens on **TCP port 3000** using **Content-Length framing** (same as the Language Server Protocol).  
Most MCP clients that support the `stdio` transport can connect via a small TCP-bridge command.

### Claude Desktop

Edit your Claude Desktop config file:
- **Windows:** `%APPDATA%\Claude\claude_desktop_config.json`
- **macOS:** `~/Library/Application Support/Claude/claude_desktop_config.json`

**Windows** (using `ncat` from [Nmap](https://nmap.org/download.html)):
```json
{
  "mcpServers": {
    "unreal": {
      "command": "ncat",
      "args": ["127.0.0.1", "3000"]
    }
  }
}
```

**macOS / Linux** (using `nc`):
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

After saving the config, restart Claude Desktop. You should see **"unreal"** listed under connected MCP servers.

> **Windows note:** The built-in `nc` / `netcat` is not available on Windows.  
> Install **Nmap** (which bundles `ncat`) from https://nmap.org/download.html, or use **WSL** and point the command at `wsl nc 127.0.0.1 3000`.

### Cursor

Open **Cursor → Settings → MCP** and add a new server entry:

```json
{
  "mcpServers": {
    "unreal": {
      "command": "ncat",
      "args": ["127.0.0.1", "3000"]
    }
  }
}
```

On macOS/Linux replace `ncat` with `nc`.

### Gemini CLI

Add this to your Gemini CLI settings file:
- **Global:** `~/.gemini/settings.json`
- **Project-local:** `.gemini/settings.json`

```json
{
  "mcpServers": {
    "unreal": {
      "command": "ncat",
      "args": ["127.0.0.1", "3000"]
    }
  }
}
```

On macOS/Linux replace `ncat` with `nc`, then restart Gemini CLI.
You can run `/mcp` in Gemini CLI to verify the server is connected.

### VS Code (with GitHub Copilot or other MCP extensions)

Add to your `.vscode/mcp.json` (or user settings under `mcp.servers`):

```json
{
  "servers": {
    "unreal": {
      "type": "stdio",
      "command": "ncat",
      "args": ["127.0.0.1", "3000"]
    }
  }
}
```

### Manual testing with PowerShell (Windows)

```powershell
$tcp = [System.Net.Sockets.TcpClient]::new("127.0.0.1", 3000)
$stream = $tcp.GetStream()

# Send initialize
$msg = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'
$frame = "Content-Length: $($msg.Length)`r`n`r`n$msg"
$bytes = [System.Text.Encoding]::UTF8.GetBytes($frame)
$stream.Write($bytes, 0, $bytes.Length)

# Read response (allow up to 500 ms for the server to reply)
Start-Sleep -Milliseconds 500
$buf = New-Object byte[] 65536
$n = $stream.Read($buf, 0, $buf.Length)
[System.Text.Encoding]::UTF8.GetString($buf, 0, $n)

$tcp.Close()
```

### Example tool calls

**List all Static Mesh actors:**
```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"get_actors_in_level","arguments":{"class_filter":"StaticMeshActor","max_results":100}}}
```

**Create a point light:**
```json
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"create_actor","arguments":{"class":"PointLight","name":"MyLight","location":{"x":0,"y":0,"z":300}}}}
```

**Get editor info:**
```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_editor_info","arguments":{}}}
```

### Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Connection refused on port 3000 | Plugin not loaded or server not started | Check Output Log for `[MCP] Server listening on port 3000` |
| `ncat` not found on Windows | ncat not installed | Install Nmap from https://nmap.org/download.html |
| Client shows no tools | `tools/list` not sent after `initialize` | Ensure the MCP client sends `initialized` notification |
| Responses are garbled | Missing Content-Length framing | Use a bridge tool (`ncat`/`nc`) instead of raw `telnet` |

---

## Building

Requires **Unreal Engine 5.3+** with the following modules (all included in the engine):

- `Sockets`, `Networking`, `Json`, `JsonUtilities`
- `UnrealEd`, `BlueprintGraph`, `Kismet`, `KismetCompiler`
- `AssetTools`, `AssetRegistry`, `LevelEditor`

### Building on Windows

#### Prerequisites

1. **Unreal Engine 5.3 or later** – Install via the Epic Games Launcher or build from source.
2. **Visual Studio 2022** (Community, Professional, or Enterprise) with the following workloads:
   - **Desktop development with C++**
   - **Game development with C++** (optional but recommended)
3. **.NET 6.0 SDK or later** – Required by UnrealBuildTool.

> **Tip:** When installing Visual Studio, make sure to include the **C++ CMake tools for Windows** and **Windows 10/11 SDK** components.

#### Step-by-step

1. **Copy the plugin** into your project's `Plugins` folder (see [Installation](#installation)).

2. **Generate Visual Studio project files**  
   Right-click your `.uproject` file in Windows Explorer and select  
   **"Generate Visual Studio project files"**.  
   This runs `UnrealBuildTool` and creates a `.sln` file next to your `.uproject`.

   Alternatively, run from a Developer Command Prompt:
   ```bat
   "C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" ^
       -projectfiles -project="C:\Path\To\MyProject\MyProject.uproject" -game -rocket -progress
   ```

3. **Open the solution** – Double-click the generated `MyProject.sln` in Visual Studio 2022.

4. **Set the build configuration**  
   In the Visual Studio toolbar, select:
   - Configuration: **Development Editor**
   - Platform: **Win64**

5. **Build**  
   Press **Ctrl+Shift+B** or right-click the project in Solution Explorer and choose **Build**.  
   The first build can take 5–20 minutes depending on your hardware.

6. **Launch the editor**  
   After a successful build, press **F5** (or click **Local Windows Debugger**) to start the Unreal Editor.  
   The **UnrealMCP** plugin will automatically start the TCP server on port **3000**.

#### Rebuilding after plugin changes

After editing plugin source files, rebuild only the Editor target:

```bat
"C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" ^
    MyProjectEditor Win64 Development ^
    "C:\Path\To\MyProject\MyProject.uproject" -WaitMutex -FromMsBuild
```

#### Verifying the server is running

Open **PowerShell** and run:

```powershell
Test-NetConnection -ComputerName 127.0.0.1 -Port 3000
```

You should see `TcpTestSucceeded : True`.  
You can also send a raw ping:

```powershell
$tcp = [System.Net.Sockets.TcpClient]::new("127.0.0.1", 3000)
$stream = $tcp.GetStream()
$msg = '{"jsonrpc":"2.0","id":1,"method":"ping","params":{}}'
$frame = "Content-Length: $($msg.Length)`r`n`r`n$msg"
$bytes = [System.Text.Encoding]::UTF8.GetBytes($frame)
$stream.Write($bytes, 0, $bytes.Length)
$buf = New-Object byte[] 4096
$n = $stream.Read($buf, 0, $buf.Length)
[System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
$tcp.Close()
```

Expected output contains `"result":{"message":"pong"}`.

- **Project plugin install**: build `<YourProjectName>Editor` in `Development Editor`.
- **Engine plugin install**: build your project `Editor` target, or build `UnrealEditor` if using a source-built engine.

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
