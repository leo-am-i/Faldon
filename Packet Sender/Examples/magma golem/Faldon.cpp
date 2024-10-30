//This code belongs to ClearMan
//do not erase this

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <thread>
#include <chrono>
#include <conio.h>
#include <psapi.h>
#include <string>
#include <array>

#pragma comment(lib, "Ws2_32.lib")

class SocketSender {
public:
    SocketSender(DWORD pid, HANDLE handle) : client_pid(pid), client_socket_handle(handle) {
        // Inicializa Winsock
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            std::cerr << "WSAStartup failed: " << iResult << std::endl;
            exit(1);
        }

        // Duplicar o handle do socket
        if (!DuplicateSocketHandle()) {
            //std::cerr << "Searching for open socket id" << std::endl;
            WSACleanup();
            //exit(1);
        }
    }

    ~SocketSender() {
        closesocket(duplicated_socket);
        WSACleanup();
    }

    bool send_packet(const unsigned char* payload, size_t payload_size) {
        int bytesSent = send(duplicated_socket, reinterpret_cast<const char*>(payload), payload_size, 0);
        if (bytesSent == SOCKET_ERROR) {
            //std::cerr << "Searching for open socket id" << std::endl;
            return false;
        }
        std::cout << "Packet sent" << std::endl;
        return true;
    }

private:
    DWORD client_pid;
    HANDLE client_socket_handle;
    SOCKET duplicated_socket;

    bool DuplicateSocketHandle() {
        HANDLE processHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, client_pid);
        if (processHandle == NULL) {
            std::cerr << "OpenProcess failed: " << GetLastError() << std::endl;
            return false;
        }

        if (!DuplicateHandle(processHandle, client_socket_handle, GetCurrentProcess(), reinterpret_cast<LPHANDLE>(&duplicated_socket), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            //std::cerr << "Searching for open socket id" << std::endl;
            CloseHandle(processHandle);
            return false;
        }

        CloseHandle(processHandle);
        return true;
    }
};

HANDLE socketFinder(DWORD client_pid, const unsigned char* payload, size_t payload_size) {
    HANDLE socket_handle = reinterpret_cast<HANDLE>(1); // Iniciando com 1
    while (true) {
        try {
            SocketSender sender(client_pid, socket_handle);
            if (sender.send_packet(payload, payload_size)) {
                //std::cout << "Open socket id found: " << int(socket_handle) << std::endl;
                return socket_handle;
            }
        }
        catch (...) {
            // Ignorar erros e continuar tentando
        }
        socket_handle = reinterpret_cast<HANDLE>(reinterpret_cast<uintptr_t>(socket_handle) + 1);
    }
}

DWORD GetPIDByName(const std::wstring& processName) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create snapshot: " << GetLastError() << std::endl;
        return pid;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (processName == entry.szExeFile) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    else {
        std::cerr << "Failed to retrieve process information: " << GetLastError() << std::endl;
    }

    CloseHandle(snap);

    if (pid != 0) {
        //std::wcout << L"PID of " << processName << L" found: " << pid << std::endl;
    }
    else {
        std::wcout << L"Process " << processName << L" not found." << std::endl;
        exit(1);
    }

    return pid;
}

void killMagma(SocketSender& sender) {
    unsigned char magma[] = { 0x83, 0x0A, 0xCA, 0x01, 0xFF };

    while (true) {
        sender.send_packet(magma, sizeof(magma));
        std::this_thread::sleep_for(std::chrono::milliseconds(345));
    }
}

int main() {
    std::wstring processName = L"Faldon.Client.exe";
    DWORD client_pid = GetPIDByName(processName);

    unsigned char findSocketPacket[] = { 0x83, 0x01, 0x61, 0x61, 0x61 };
    HANDLE client_socket_handle = socketFinder(client_pid, findSocketPacket, sizeof(findSocketPacket));

    SocketSender sender(client_pid, client_socket_handle);

    system("cls");
    std::cout << "Running..." << std::endl;

    std::thread killMagma_thread(killMagma, std::ref(sender));
    killMagma_thread.join();

    return 0;
}