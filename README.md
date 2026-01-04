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

## Running with Classic Command Prompt (conhost)
On Windows 11, the system often defaults to "Windows Terminal." If you prefer the classic Command Prompt look or need the specific window behavior shown in the demonstration, you can force the application to run using `conhost.exe`.
### Steps to Configure:
1. **Create a Shortcut**: Right-click **BotManager.exe** and select **Create Shortcut**.
2. **Open Properties**: Right-click the new shortcut and select **Properties**.
3. **Modify the Target**: In the **Target** field, add `conhost.exe` followed by a space at the very beginning of the existing path.
4. **Save and Run**: Click **OK**. You can always use this shortcut to launch the program.
### Example Target Path:
> `conhost.exe C:\RO_PreRenewal\4_Openkore\BotManager.exe`
## Why use conhost.exe?
Using the classic console host ensures that the program's window positioning and sizing logic works exactly as intended, especially if you have customized the grid layout in your `.ini` settings.

## Build Requirements
* **Compiler**: Visual Studio (MSVC)
* **Standard**: C++17
* **Libraries**: Link with `Psapi.lib` and `ntdll.lib`
