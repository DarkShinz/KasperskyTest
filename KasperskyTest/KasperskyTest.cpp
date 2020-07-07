#include <windows.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <chrono>

using namespace std;

void throwError(string text, string title = "Error!")
{
    wstring t(title.begin(), title.end());
    wstring txt(text.begin(), text.end());
    MessageBox(0, txt.c_str(), t.c_str(), MB_OK | MB_ICONEXCLAMATION);
}

bool FileExist(const string& name) 
{
    ifstream f(name.c_str());
    bool ok = f.good();
    f.close();
    return ok;
}

void ExecutablePath(char* argv[], std::string& path, std::string& filename)
{
    std::string argv_str(argv[0]);
    size_t lastSlash = argv_str.find_last_of("\\");
    path = argv_str.substr(0, lastSlash);
    filename = argv_str.substr(lastSlash, argv_str.find_last_of(".") - lastSlash);
}

BOOL Is64BitWindows()
{
#if defined(_WIN64)
    return TRUE;  // 64-bit programs run only on Win64
#elif defined(_WIN32)
    // 32-bit programs run on both 32-bit and 64-bit Windows
    // so must sniff
    BOOL f64 = FALSE;
    return IsWow64Process(GetCurrentProcess(), &f64) && f64;
#else
    return FALSE; // Win64 does not support Win16
#endif
}

BOOL CreateWinPipe(HANDLE& pipe, string& pipe_name)
{
    pipe = CreateNamedPipeA(&pipe_name[0], PIPE_ACCESS_OUTBOUND, PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 2, 0, 0, 0, 0);
    if ((HANDLE)-1 == pipe)
    {
        CloseHandle(pipe);
        return false;
    }
    return true;
}

int main(int argc, char* argv[])
{
    // Load
    srand((unsigned)time(0));

    std::string exePath, filename, ahkPath, cmdline;
    std::string targetName = "\\init.ahk";
    ExecutablePath(argv, exePath, filename);

    //Look for autohotkey.exe
    if (!FileExist(exePath + "\\assets\\AutoHotkeyU64.exe") && !FileExist(exePath + "\\assets\\AutoHotkeyU32.exe"))
    {
        throwError("AutoHotkey.exe not found inside assets folder.");
        return -1;
    }

    ahkPath = exePath + (Is64BitWindows() ? "\\assets\\AutoHotkeyU64.exe" : "\\assets\\AutoHotkeyU32.exe");
    cmdline = "\"" + ahkPath + "\"";


    cmdline += " \"\\\\.\\pipe\\" + filename + "\" \"" + exePath + targetName + "\"";
    std::string pipe_name = "\\\\.\\pipe\\" + filename;
    std::wstring base = L"Kernel32";
    HANDLE p1 = 0, p2 = 0;

    if (!CreateWinPipe(p1, pipe_name))
    {
        throwError("Falha ao iniciar aplicativo.");
        return -5;
    }

    if (!CreateWinPipe(p2, pipe_name))
    {
        throwError("Falha ao iniciar aplicativo.");
        return -7;
    }

    _STARTUPINFOA startupInfo;
    _PROCESS_INFORMATION processInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    ZeroMemory(&processInfo, sizeof(processInfo));
    startupInfo.cb = sizeof(startupInfo);

    if (!CreateProcessA(0, &cmdline[0], 0, 0, 0, 0, 0, 0, &startupInfo, &processInfo))
    {
        throwError("Este aplicativo falhou ao iniciar.");
        CloseHandle(p1);
        return -6;
    }
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    ConnectNamedPipe(p1, 0);
    CloseHandle(p1);

    ConnectNamedPipe(p2, 0);

    int writeIndex = 0;
    string program = "MsgBox, Hello World!";
    int bufferSize = program.length() + 3;
    char* buff = new char[bufferSize];
    memset(buff, 0, sizeof(buff));
    writeIndex = 3;
    buff[0] = 0xEF;
    buff[1] = 0xBB;
    buff[2] = 0xBF;

    memcpy(&buff[writeIndex], &program[0], program.length()+1);
    writeIndex += program.length();

    DWORD bytesWritten = 0;
    WriteFile(p2, &buff[0], writeIndex - 1, &bytesWritten, 0);

    memset(buff, 0, bufferSize);

    delete[] buff;
    buff = 0;

    CloseHandle(p2);
    return 0;
}