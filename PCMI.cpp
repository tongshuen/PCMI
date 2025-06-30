/*
 * PCMI (PowerCommandInterpreter)
 * Copyright (C) 2025 童顺
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <windows.h>
#include <iostream>
#include <string>
#include <direct.h>
#include <cstdlib>
#include <signal.h>
#include <atomic>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>
#include <shlobj.h>
#include <fstream>
#include <vector>

#define FILE_EXISTS(path) (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)

std::atomic<bool> stopCommand(false);
const std::string LOG_FILE = "PCMI.log";
const std::string SHUTDOWN_FLAG_FILE = "Properly_shut_down.pcmi";
const std::string CONTEXT_FILE = "Context.pcmi";
const std::string PCML_EXTENSION = ".pcml";
const std::string PCML_REGISTRY_KEY = "PCMI.pcml";
const std::string REGISTERED_FLAG = "Registered.pcmi";

std::string GetCurrentDateTime();
void ExecuteCommand(const std::string& command);
void ExecutePCMLFile(const std::string& filePath);
void PrintStatus(const std::string& message, bool success);

class Logger {
private:
    static std::vector<std::string> logBuffer;
    static bool memoryLow;

public:
    static void Initialize() {
        std::cout << "正在启动 PCMI..." << std::endl;
        
        PrintStatus("读取 " + SHUTDOWN_FLAG_FILE, FILE_EXISTS(SHUTDOWN_FLAG_FILE));
        
        std::ofstream shutdownFile(SHUTDOWN_FLAG_FILE.c_str());
        if (shutdownFile.is_open()) {
            shutdownFile << "False";
            PrintStatus("更改 " + SHUTDOWN_FLAG_FILE + " 为 False", true);
        } else {
            PrintStatus("更改 " + SHUTDOWN_FLAG_FILE + " 为 False", false);
        }
        shutdownFile.close();
        
        std::string savedDir = LoadContext();
        if (!savedDir.empty()) {
            if (_chdir(savedDir.c_str()) == 0) {
                PrintStatus("恢复上下文到目录: " + savedDir, true);
            } else {
                PrintStatus("恢复上下文到目录: " + savedDir, false);
            }
        }
        
        std::cout << "现在进入！" << std::endl;
        system("cls");
        logBuffer.push_back("PCMI 初始化于 " + GetCurrentDateTime());
    }
    
    static void Log(const std::string& message) {
        try {
            logBuffer.push_back(message);
        } catch (const std::bad_alloc&) {
            memoryLow = true;
            FlushLogs();
            logBuffer.push_back(message);
        }
    }
    
    static void FlushLogs() {
        std::ofstream logFile(LOG_FILE.c_str(), std::ios::app);
        if (logFile.is_open()) {
            for (const auto& msg : logBuffer) {
                logFile << msg << "\n";
            }
            logBuffer.clear();
        }
    }
    
    static void Shutdown() {
        std::cout << "正在关闭 PCMI..." << std::endl;
        
        FlushLogs();
        
        std::ofstream shutdownFile(SHUTDOWN_FLAG_FILE.c_str());
        if (shutdownFile.is_open()) {
            shutdownFile << "True";
            PrintStatus("更改 " + SHUTDOWN_FLAG_FILE + " 为 True", true);
        } else {
            PrintStatus("更改 " + SHUTDOWN_FLAG_FILE + " 为 True", false);
        }
        shutdownFile.close();
        
        std::cout << "现在退出！" << std::endl;
    }
    
    static bool WasProperlyShutdown() {
        if (!FILE_EXISTS(SHUTDOWN_FLAG_FILE)) {
            return false;
        }
        
        std::ifstream shutdownFile(SHUTDOWN_FLAG_FILE.c_str());
        std::string content;
        if (shutdownFile.is_open()) {
            shutdownFile >> content;
            return content == "True";
        }
        return false;
    }
    
    static void SaveContext(const std::string& currentDir) {
        std::ofstream contextFile(CONTEXT_FILE.c_str());
        if (contextFile.is_open()) {
            contextFile << currentDir;
        }
    }
    
    static std::string LoadContext() {
        if (!FILE_EXISTS(CONTEXT_FILE)) {
            return "";
        }
        
        std::ifstream contextFile(CONTEXT_FILE.c_str());
        std::string content;
        if (contextFile.is_open()) {
            std::getline(contextFile, content);
            return content;
        }
        return "";
    }
    
    static bool IsRegistered() {
        return FILE_EXISTS(REGISTERED_FLAG);
    }
    
    static void MarkAsRegistered() {
        std::ofstream flagFile(REGISTERED_FLAG.c_str());
        if (flagFile.is_open()) {
            flagFile << "True";
        }
    }
};

std::vector<std::string> Logger::logBuffer;
bool Logger::memoryLow = false;

void SetConsoleColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void PrintStatus(const std::string& message, bool success) {
    SetConsoleColor(success ? 10 : 12);
    std::cout << "[" << (success ? "OK" : "FAIL") << "] " << message << std::endl;
    SetConsoleColor(10);
}

std::string GetCurrentDateTime() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y:%m:%d:%H:%M:%S", &tstruct);
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    char result[85];
    sprintf(result, "%s.%03d", buf, st.wMilliseconds);
    
    return std::string(result);
}

std::string GetCurrentDirectoryPath() {
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    return std::string(buffer);
}

std::string GetExecutablePath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    return std::string(buffer);
}

bool RegisterFileAssociation() {
    HKEY hKey;
    std::string exePath = GetExecutablePath();
    
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, PCML_REGISTRY_KEY.c_str(), 0, NULL, 
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    
    std::string friendlyName = "PCMI Command Script";
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)friendlyName.c_str(), friendlyName.size() + 1);
    RegCloseKey(hKey);
    
    std::string iconKey = PCML_REGISTRY_KEY + "\\DefaultIcon";
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, iconKey.c_str(), 0, NULL, 
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    
    std::string iconValue = exePath + ",0";
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)iconValue.c_str(), iconValue.size() + 1);
    RegCloseKey(hKey);
    
    std::string commandKey = PCML_REGISTRY_KEY + "\\shell\\open\\command";
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, commandKey.c_str(), 0, NULL, 
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    
    std::string commandValue = "\"" + exePath + "\" \"%1\"";
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)commandValue.c_str(), commandValue.size() + 1);
    RegCloseKey(hKey);
    
    if (RegCreateKeyExA(HKEY_CLASSES_ROOT, PCML_EXTENSION.c_str(), 0, NULL, 
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }
    
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)PCML_REGISTRY_KEY.c_str(), PCML_REGISTRY_KEY.size() + 1);
    RegCloseKey(hKey);
    
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    
    Logger::MarkAsRegistered();
    return true;
}

void ExecutePCMLFile(const std::string& filePath) {
    std::ifstream file(filePath.c_str());
    if (!file.is_open()) {
        SetConsoleColor(12);
        std::cout << "错误: 无法打开文件 " << filePath << std::endl;
        SetConsoleColor(10);
        return;
    }
    
    bool echoOn = true;
    std::string line;
    std::getline(file, line);
    
    if (line.find("@echo off") != std::string::npos) {
        echoOn = false;
    } else {
        if (!line.empty()) {
            if (echoOn) {
                SetConsoleColor(14);
                std::cout << filePath << "> " << line << std::endl;
                SetConsoleColor(10);
            }
            system(line.c_str());
        }
    }
    
    while (std::getline(file, line)) {
        if (stopCommand) {
            stopCommand = false;
            break;
        }
        
        if (line.empty() || line[0] == '@') {
            continue;
        }
        
        if (echoOn) {
            SetConsoleColor(14);
            std::cout << filePath << "> " << line << std::endl;
            SetConsoleColor(10);
        }
        
        system(line.c_str());
    }
    
    file.close();
}

void ExecuteCommand(const std::string& command) {
    stopCommand = false;
    
    if (command.substr(0, 3) == "cd ") {
        std::string path = command.substr(3);
        
        size_t pos;
        while ((pos = path.find('/')) != std::string::npos) {
            path.replace(pos, 1, "\\");
        }
        
        if (!path.empty() && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.length() - 2);
        }
        
        if (path == "/" || path == "\\") {
            path = "\\";
        }
        else if (path == ".") {
            return;
        }
        else if (path == "..") {
            char currentDir[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, currentDir);
            std::string parentDir(currentDir);
            size_t lastSlash = parentDir.find_last_of("\\");
            if (lastSlash != std::string::npos) {
                path = parentDir.substr(0, lastSlash);
                if (path.empty()) path = "\\";
            }
        }
        
        if (_chdir(path.c_str()) != 0) {
            std::string errorMsg = "错误: 无法切换到目录 \"" + path + "\"";
            Logger::Log(errorMsg);
            SetConsoleColor(12);
            std::cout << errorMsg << std::endl;
            SetConsoleColor(10);
        }
    }
    else {
        std::string cmd = "cmd /c " + command;
        Logger::Log("Executing: " + command);
        
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) {
            std::string errorMsg = "错误: 无法执行命令";
            Logger::Log(errorMsg);
            SetConsoleColor(12);
            std::cout << errorMsg << std::endl;
            SetConsoleColor(10);
            return;
        }
        
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string output(buffer);
            Logger::Log(output);
            std::cout << output;
        }
        
        _pclose(pipe);
    }
}

void ParseCommand(const std::string& command) {
    Logger::Log("Command: " + (command.empty() ? "[empty]" : command));
    
    if (command == "help") {
        SetConsoleColor(14);
        std::cout << "可用命令:\n";
        std::cout << "help - 显示帮助信息\n";
        std::cout << "pcmi - 显示PCMI信息\n";
        std::cout << "exit - 退出PCMI\n";
        std::cout << "netstat - 显示网络状态\n";
        std::cout << "route - 显示路由表\n";
        std::cout << "register - 注册.pcml文件关联\n";
        std::cout << "其他命令和 CMD 相同\n";
        SetConsoleColor(10);
    } 
    else if (command == "cls") {
        Logger::Log("Clearing screen");
        system("cls");
    }
    else if (command == "pcmi") {
        std::string info = "PCMI ，全称 PowerCommandInterpreter ，由童顺开发，使用C++编写，采用GPL-v3开源\nwww.tongshunham.top/PCMI/";
        Logger::Log(info);
        SetConsoleColor(14);
        std::cout << info << std::endl;
        SetConsoleColor(10);
    }
    else if (command == "exit") {
        Logger::Shutdown();
        exit(0);
    }
    else if (command == "netstat") {
        ExecuteCommand("netstat -ano");
    }
    else if (command == "route") {
        ExecuteCommand("route print");
    }
    else if (command == "register") {
        if (RegisterFileAssociation()) {
            SetConsoleColor(14);
            std::cout << "成功注册.pcml文件关联" << std::endl;
            SetConsoleColor(10);
        } else {
            SetConsoleColor(12);
            std::cout << "错误: 无法注册.pcml文件关联" << std::endl;
            SetConsoleColor(10);
        }
    }
    else if (command.substr(0, 3) == "cd ") {
        ExecuteCommand(command);
    }
    else if (command == "cd") {
        std::string currentDir = GetCurrentDirectoryPath();
        Logger::Log(currentDir);
        SetConsoleColor(14);
        std::cout << currentDir << std::endl;
        SetConsoleColor(10);
    }
    else if (command.length() > 5 && command.substr(command.length() - 5) == ".pcml") {
        ExecutePCMLFile(command);
    }
    else {
        ExecuteCommand(command);
    }
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT) {
        stopCommand = true;
        Logger::Log("Command interrupted by Ctrl+C");
        return TRUE;
    }
    return FALSE;
}

void PrintPrompt() {
    std::string currentDir = GetCurrentDirectoryPath();
    std::string prompt = "PCMI [" + GetCurrentDateTime() + "] [" + currentDir + "]:> ";
    
    Logger::Log(prompt.substr(0, prompt.size()));
    
    SetConsoleColor(11);
    std::cout << prompt;
    SetConsoleColor(10);
}

int main(int argc, char* argv[]) {
    Logger::Initialize();
    
    if (!Logger::IsRegistered()) {
        SetConsoleColor(14);
        std::cout << "首次运行，正在注册.pcml文件关联..." << std::endl;
        if (RegisterFileAssociation()) {
            std::cout << "成功注册.pcml文件关联" << std::endl;
        } else {
            std::cout << "警告: 无法注册.pcml文件关联" << std::endl;
            std::cout << "您可以稍后手动输入'register'命令尝试重新注册" << std::endl;
        }
        SetConsoleColor(10);
    }
    
    Logger::SaveContext(GetCurrentDirectoryPath());
    
    SetConsoleTitle("PCMI (C)2025 童顺 www.tongshunham.top");
    SetConsoleOutputCP(936);
    SetConsoleColor(10);

    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        Logger::Log("错误: 无法设置Ctrl+C处理程序");
        SetConsoleColor(12);
        std::cout << "错误: 无法设置Ctrl+C处理程序" << std::endl;
        SetConsoleColor(10);
        return 1;
    }

    std::string welcomeMsg = "(C)2025 童顺 \nwww.tongshunham.top\n输入help查看教程\n数据无价，谨慎操作。发现异常，立刻备份！文件删除，三思后行。危险命令，手指口呼。双次确认，保证无误。";
    Logger::Log(welcomeMsg);
    SetConsoleColor(11);
    std::cout << welcomeMsg << std::endl;
    SetConsoleColor(10);
    
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg.length() > 5 && arg.substr(arg.length() - 5) == ".pcml") {
            ExecutePCMLFile(arg);
            Logger::Shutdown();
            return 0;
        }
    }
    
    std::string command;
    while (true) {
        PrintPrompt();
        std::getline(std::cin, command);
        
        Logger::SaveContext(GetCurrentDirectoryPath());
        
        ParseCommand(command);
    }

    return 0;
}
