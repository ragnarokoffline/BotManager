# Bot Manager for OpenKore

A lightweight C++ console utility to launch, monitor, and manage multiple bot instances from a single dashboard.
This program is part of the Ragnarok Offline Pack I released on my YouTube Channel https://www.youtube.com/@wrywndp

<br/><br/>
<p align="center">
   <img src="Screenshot/BotManager_Screenshot_1.png">
<p align="center">
<br/><br/>

## Showcase
https://youtu.be/UZGHdU8bsTw

## Features
* **Status Dashboard**: Real-time `[ON]` / `[  ]` status monitoring in a grid layout.
* **Process Detection**: Identifies unique instances by inspecting `--control` paths in memory.
* **Batch Operations**: Start or end bots using IDs or ranges (e.g., `1 3 5-10`).
* **Persistence**: Saves window position and settings to `BotManagerConfig.ini`.

## How to Use
1. **Setup**: Create `BotManagerPaths.txt` and list your bot folder paths (one per line).
2. **Configure**: Set your `StarterExecutable` (e.g., `start.exe`) in `BotManagerConfig.ini`.
3. **Commands**: 
    * `5`: Start bot 5.
    * `1-10`: Start range 1 to 10.
    * `e5`: Terminate bot 5.
    * `e1-10`: Terminate range 1 to 10.
    * `Q` / `E`: Run All / End All.
    * `exit`: Save settings and close.

## Technical Note
This tool uses `ReadProcessMemory` to distinguish between multiple bots running the same executable name. This may trigger **False Positive** alerts in antivirus software. Because the source is provided, you can verify that these calls are used strictly for process identification.

## Troubleshooting
* **Status not updating**: If your bots are "Run as Administrator," the Manager must also be "Run as Administrator."
* **Antivirus**: If the `.exe` is deleted, add the folder to your antivirus exclusions list.

## Build Requirements
* **Compiler**: Visual Studio (MSVC)
* **Standard**: C++17
* **Libraries**: Link with `Psapi.lib` and `ntdll.lib`
