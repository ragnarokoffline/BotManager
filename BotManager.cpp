#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <chrono>
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <conio.h>
#include <process.h>

namespace fs = std::filesystem;

typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
    );

struct Config {
    std::string starter = "start.exe";
    std::string botListFile = "BotManagerPaths.txt";
    int botsPerColumn = 20;
    double launchDelay = 0.5;
    int windowX = 200;
    int windowY = 200;
};

struct Bot {
    int id;
    std::string path;
    std::string displayName;
    bool isRunning = false;
};

std::string g_iniFile = ".\\BotManagerConfig.ini";
Config g_cfg;
std::string g_inputBuffer = "";
int g_origX = -1, g_origY = -1;

// --- NEW FEATURE: ENSURE SETTINGS EXIST ---
void EnsureSettingsExist() {
    if (!fs::exists(g_iniFile) || fs::file_size(g_iniFile) == 0) {
        std::ofstream outfile(g_iniFile, std::ios::trunc);
        outfile << "[Settings]\n"
            << "; The name of your OpenKore loader (e.g., start.exe or wxstart.exe)\n"
            << "StarterExecutable=start.exe\n\n"
            << "; The text file containing the list of your bot folder paths\n"
            << "BotListFile=BotManagerPaths.txt\n\n"
            << "; Seconds to wait between starting each bot (use 0 for no delay)\n"
            << "LaunchDelay=0.5\n\n"
            << "[Interface]\n"
            << "; Number of bots to display in a single vertical column\n"
            << "BotsPerColumn=20\n\n"
            << "; Horizontal position of the window on your screen (auto-saved)\n"
            << "WindowX=200\n\n"
            << "; Vertical position of the window on your screen (auto-saved)\n"
            << "WindowY=200\n";
        outfile.close();
    }
}

void resetCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD cursorPosition = { 0, 0 };
    SetConsoleCursorPosition(hConsole, cursorPosition);
}

std::string getCommandLine(HANDLE hProcess) {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return "";
    auto NtQueryInformationProcess = (pNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");
    if (!NtQueryInformationProcess) return "";
    PROCESS_BASIC_INFORMATION pbi;
    ULONG len;
    if (NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &len) != 0) return "";
    PEB peb;
    if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(peb), NULL)) return "";
    RTL_USER_PROCESS_PARAMETERS params;
    if (!ReadProcessMemory(hProcess, peb.ProcessParameters, &params, sizeof(params), NULL)) return "";
    std::vector<WCHAR> buffer(params.CommandLine.Length / sizeof(WCHAR) + 1);
    if (!ReadProcessMemory(hProcess, params.CommandLine.Buffer, buffer.data(), params.CommandLine.Length, NULL)) return "";
    buffer[params.CommandLine.Length / sizeof(WCHAR)] = L'\0';
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, &strTo[0], size_needed, NULL, NULL);
    if (!strTo.empty() && strTo.back() == '\0') strTo.pop_back();
    return strTo;
}

void updateBotStatus(std::vector<Bot>& bots) {
    for (auto& bot : bots) bot.isRunning = false;
    DWORD processes[1024], cbNeeded;
    if (!EnumProcesses(processes, sizeof(processes), &cbNeeded)) return;
    DWORD count = cbNeeded / sizeof(DWORD);
    for (DWORD i = 0; i < count; i++) {
        if (processes[i] == 0) continue;
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
        if (hProcess) {
            char name[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, NULL, name, sizeof(name))) {
                if (std::string(name) == g_cfg.starter) {
                    std::string cmd = getCommandLine(hProcess);
                    for (auto& bot : bots) {
                        if (cmd.find("--control=\"" + bot.path + "\"") != std::string::npos ||
                            cmd.find("--control=" + bot.path) != std::string::npos) {
                            bot.isRunning = true;
                        }
                    }
                }
            }
            CloseHandle(hProcess);
        }
    }
}

bool killOneBot(const Bot& bot, bool silent = false) {
    bool killed = false;
    DWORD processes[1024], cbNeeded;
    if (!EnumProcesses(processes, sizeof(processes), &cbNeeded)) return false;
    DWORD count = cbNeeded / sizeof(DWORD);
    for (DWORD i = 0; i < count; i++) {
        if (processes[i] == 0) continue;
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
        if (hProcess) {
            char name[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, NULL, name, sizeof(name))) {
                if (std::string(name) == g_cfg.starter) {
                    std::string cmd = getCommandLine(hProcess);
                    if (cmd.find("--control=\"" + bot.path + "\"") != std::string::npos ||
                        cmd.find("--control=" + bot.path) != std::string::npos) {
                        if (!silent) std::cout << "  Terminating: " << std::left << std::setw(20) << bot.displayName << " [OK]" << std::endl;
                        TerminateProcess(hProcess, 0);
                        killed = true;
                    }
                }
            }
            CloseHandle(hProcess);
        }
    }
    return killed;
}

void saveSettingsIfChanged() {
    HWND hwnd = GetConsoleWindow();
    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        if (rect.left == g_origX && rect.top == g_origY) return;
        WritePrivateProfileStringA("Interface", "WindowX", std::to_string(rect.left).c_str(), g_iniFile.c_str());
        WritePrivateProfileStringA("Interface", "WindowY", std::to_string(rect.top).c_str(), g_iniFile.c_str());
        g_origX = rect.left; g_origY = rect.top;
    }
}

BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_CLOSE_EVENT) { saveSettingsIfChanged(); return TRUE; }
    return FALSE;
}

void displayMenu(std::vector<Bot>& bots) {
    updateBotStatus(bots);
    resetCursor();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(hConsole, 3);
    std::cout << " ==============================================================" << std::endl;
    std::cout << " ==============     ";
    SetConsoleTextAttribute(hConsole, 6);
    std::cout << "Bot Manager (OpenKore)";
    SetConsoleTextAttribute(hConsole, 3);
    std::cout << "     ================" << std::endl;
    std::cout << " ==============================================================\n" << std::endl;
    SetConsoleTextAttribute(hConsole, 7);

    int rows = g_cfg.botsPerColumn;
    int totalBots = (int)bots.size();
    int columns = (totalBots + rows - 1) / rows;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            int idx = r + c * rows;
            if (idx < totalBots) {
                std::cout << "  " << std::right << std::setw(2) << bots[idx].id << " ";

                if (bots[idx].isRunning) {
                    SetConsoleTextAttribute(hConsole, 10);
                    std::cout << "[ON] ";
                }
                else {
                    SetConsoleTextAttribute(hConsole, 8);
                    std::cout << "[  ] ";
                }
                SetConsoleTextAttribute(hConsole, 7);

                std::string shortName = bots[idx].displayName;
                if (shortName.length() > 18) {
                    shortName = shortName.substr(0, 15) + "...";
                }
                std::cout << std::left << std::setw(20) << shortName;
            }
        }
        std::cout << std::endl;
    }

    SetConsoleTextAttribute(hConsole, 3);
    std::cout << "\n ==============================================================" << std::endl;

    SetConsoleTextAttribute(hConsole, 7);
    std::cout << "  Run All : ";
    SetConsoleTextAttribute(hConsole, 10);
    std::cout << "Q";
    SetConsoleTextAttribute(hConsole, 7);
    std::cout << "         |  ";
    std::cout << "End All : ";
    SetConsoleTextAttribute(hConsole, 12);
    std::cout << "E" << std::endl;

    SetConsoleTextAttribute(hConsole, 7);
    std::cout << "  Start   : ";
    SetConsoleTextAttribute(hConsole, 11);
    std::cout << "1 3 5-10";
    SetConsoleTextAttribute(hConsole, 7);
    std::cout << "  |  ";
    std::cout << "End     : ";
    SetConsoleTextAttribute(hConsole, 12);
    std::cout << "e1 e3 e5-10" << std::endl;

    SetConsoleTextAttribute(hConsole, 3);
    std::cout << " ==============================================================\n" << std::endl;

    SetConsoleTextAttribute(hConsole, 14);
    std::cout << "  Enter your choice: ";
    SetConsoleTextAttribute(hConsole, 7);
    std::cout << g_inputBuffer << std::flush;
}

void runOneBot(const Bot& bot, const std::string& baseDir) {
    if (bot.isRunning) {
        std::cout << "  Bot: " << std::left << std::setw(25) << bot.displayName << "... Already Running." << std::endl;
        return;
    }
    std::cout << "  Starting bot: " << std::left << std::setw(25) << bot.displayName << "... Done." << std::endl;
    std::string cmd = "start \"\" \"" + baseDir + "\\" + g_cfg.starter + "\" --control=\"" + bot.path + "\"";
    system(cmd.c_str());
    if (g_cfg.launchDelay > 0) std::this_thread::sleep_for(std::chrono::milliseconds((int)(g_cfg.launchDelay * 1000)));
}

void processChoice(std::string choice, std::vector<Bot>& bots, const std::string& baseDir) {
    bool anyAction = false;
    std::cout << "\n" << std::endl;
    std::replace(choice.begin(), choice.end(), ',', ' ');
    std::stringstream ss(choice);
    std::string seg;
    while (ss >> seg) {
        if (seg == "q" || seg == "Q") { anyAction = true; for (const auto& b : bots) runOneBot(b, baseDir); }
        else if (seg[0] == 'e' || seg[0] == 'E') {
            if (seg.length() == 1) { anyAction = true; for (const auto& b : bots) killOneBot(b, true); }
            else {
                std::string numPart = seg.substr(1);
                if (numPart.find('-') != std::string::npos) {
                    size_t d = numPart.find('-');
                    try {
                        int s = std::stoi(numPart.substr(0, d)), e = std::stoi(numPart.substr(d + 1));
                        int step = (s <= e) ? 1 : -1;
                        for (int i = s; (step == 1 ? i <= e : i >= e); i += step) {
                            if (i > 0 && i <= (int)bots.size()) { if (killOneBot(bots[i - 1])) anyAction = true; }
                        }
                    }
                    catch (...) {}
                }
                else {
                    try {
                        int i = std::stoi(numPart);
                        if (i > 0 && i <= (int)bots.size()) { if (killOneBot(bots[i - 1])) anyAction = true; }
                    }
                    catch (...) {}
                }
            }
        }
        else if (seg.find('-') != std::string::npos) {
            size_t d = seg.find('-');
            try {
                int s = std::stoi(seg.substr(0, d)), e = std::stoi(seg.substr(d + 1));
                int step = (s <= e) ? 1 : -1;
                for (int i = s; (step == 1 ? i <= e : i >= e); i += step) {
                    if (i > 0 && i <= (int)bots.size()) { runOneBot(bots[i - 1], baseDir); anyAction = true; }
                }
            }
            catch (...) {}
        }
        else {
            try {
                int i = std::stoi(seg);
                if (i > 0 && i <= (int)bots.size()) { runOneBot(bots[i - 1], baseDir); anyAction = true; }
            }
            catch (...) {}
        }
    }
    if (anyAction) { std::cout << "\n  Operation Complete." << std::endl; std::this_thread::sleep_for(std::chrono::milliseconds(1000)); }
}

int main() {
    HWND hWndInit = GetConsoleWindow();
    char className[256];
    GetClassNameA(hWndInit, className, sizeof(className));

    if (std::string(className) != "ConsoleWindowClass") {
        char exepath[MAX_PATH];
        GetModuleFileNameA(NULL, exepath, MAX_PATH);
        _spawnl(_P_NOWAIT, "C:\\Windows\\System32\\conhost.exe", "conhost.exe", exepath, NULL);
        return 0;
    }

    // Initialize Settings
    EnsureSettingsExist();
    SetConsoleCtrlHandler(consoleHandler, TRUE);

    // Load Settings using WinAPI Profile functions
    char buf[255];
    GetPrivateProfileStringA("Settings", "StarterExecutable", "start.exe", buf, 255, g_iniFile.c_str());
    g_cfg.starter = buf;

    GetPrivateProfileStringA("Settings", "BotListFile", "BotManagerPaths.txt", buf, 255, g_iniFile.c_str());
    g_cfg.botListFile = buf;

    GetPrivateProfileStringA("Settings", "LaunchDelay", "0.5", buf, 255, g_iniFile.c_str());
    g_cfg.launchDelay = std::stod(buf);

    g_cfg.botsPerColumn = GetPrivateProfileIntA("Interface", "BotsPerColumn", 20, g_iniFile.c_str());
    g_cfg.windowX = GetPrivateProfileIntA("Interface", "WindowX", 200, g_iniFile.c_str());
    g_cfg.windowY = GetPrivateProfileIntA("Interface", "WindowY", 200, g_iniFile.c_str());

    // Setup Console Environment
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    if (GetConsoleMode(hIn, &mode)) {
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        mode &= ~ENABLE_MOUSE_INPUT;
        mode &= ~ENABLE_INSERT_MODE;
        mode |= ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(hIn, mode);
    }

    std::vector<Bot> bots;
    std::ifstream bf(g_cfg.botListFile);
    int botCounter = 1;
    if (bf.is_open()) {
        std::string l;
        while (std::getline(bf, l)) {
            if (!l.empty()) {
                bots.push_back({ botCounter++, l, fs::path(l).filename().string() });
            }
        }
        bf.close();
    }

    int numRows = g_cfg.botsPerColumn;
    int numCols = ((int)bots.size() + numRows - 1) / numRows;
    int consoleWidth = 64;
    if (numCols > 2) {
        consoleWidth += ((numCols - 2) * 30);
    }

    HWND hwnd = GetConsoleWindow();
    SetWindowPos(hwnd, 0, g_cfg.windowX, g_cfg.windowY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    g_origX = g_cfg.windowX;
    g_origY = g_cfg.windowY;

    system(("mode con: cols=" + std::to_string(consoleWidth) + " lines=" + std::to_string(numRows + 12)).c_str());

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~WS_MAXIMIZEBOX;
    style &= ~WS_THICKFRAME;
    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD bufferSize = { (SHORT)consoleWidth, (SHORT)(numRows + 12) };
    SetConsoleScreenBufferSize(hOut, bufferSize);

    CONSOLE_CURSOR_INFO cursorInfo;
    cursorInfo.dwSize = 20;
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &cursorInfo);

    system("title Bot Manager 2.0.1 by wrywndp");

    auto lastRef = std::chrono::steady_clock::now();
    system("cls");
    displayMenu(bots);

    while (true) {
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastRef).count() >= 5) {
            displayMenu(bots);
            lastRef = std::chrono::steady_clock::now();
        }
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 0 || ch == 224) { (void)_getch(); continue; }
            if (ch == '\r') {
                if (g_inputBuffer.empty()) continue;
                if (g_inputBuffer == "exit") { saveSettingsIfChanged(); break; }
                processChoice(g_inputBuffer, bots, fs::current_path().string());
                g_inputBuffer = "";
                system("cls");
                displayMenu(bots);
                lastRef = std::chrono::steady_clock::now();
            }
            else if (ch == '\b') {
                if (!g_inputBuffer.empty()) {
                    g_inputBuffer.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            }
            else if (isprint(ch)) {
                g_inputBuffer += (char)ch;
                std::cout << (char)ch << std::flush;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return 0;
}