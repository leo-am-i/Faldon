//This code belongs to clear-man
//do not erase this

#include <iostream>
#include <winsock2.h>
#include <tlhelp32.h>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")


class SocketSender {
public:
    SocketSender(DWORD pid, HANDLE handle) : client_pid(pid), client_socket_handle(handle) {
        //Inicializa Winsock
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            std::cerr << "WSAStartup failed: " << iResult << std::endl;
            exit(1);
        }

        //Duplicar o handle do socket
        if (!DuplicateSocketHandle()) {
            //std::cerr << "Searching for open socket id" << std::endl; //Se falhou ao duplicar o handle, tentar novamente
            WSACleanup();
            //exit(1);
        }
    }

    ~SocketSender() {
        closesocket(duplicated_socket);
        WSACleanup();
    }

    bool send_packet(const unsigned char* payload, size_t payload_size) {
        int bytesSent = send(duplicated_socket, reinterpret_cast<const char*>(payload), payload_size, 0); //Função send (Winsock) para enviar pacotes
        if (bytesSent == SOCKET_ERROR) {
            //std::cerr << "Searching for open socket id" << std::endl; //Se falhou ao enviar o pacote, retorna false. Quando estiver tentando achar o socket, tentará novamente até que retorne true
            return false;
        }
        //std::cout << "Packet sent" << std::endl;
        return true;
    }

private:
    DWORD client_pid;
    HANDLE client_socket_handle;
    SOCKET duplicated_socket;

    bool DuplicateSocketHandle() { //Função para duplicar o SocketHandle
        HANDLE processHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, client_pid); //Requisição para duplicar o Handle do processo desejado
        if (processHandle == NULL) {
            std::cerr << "OpenProcess failed: " << GetLastError() << std::endl;
            return false;
        }

        if (!DuplicateHandle(processHandle, client_socket_handle, GetCurrentProcess(), reinterpret_cast<LPHANDLE>(&duplicated_socket), 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            //std::cerr << "DuplicateHandle failed: " << GetLastError() << std::endl;
            CloseHandle(processHandle);
            return false;
        }

        CloseHandle(processHandle);
        return true;
    }
};

HANDLE socketFinder(DWORD client_pid, const unsigned char* payload, size_t payload_size) { //Função para encontrar o SocketHandle (open socket id do processo) por força bruta
    HANDLE socket_handle = reinterpret_cast<HANDLE>(1); //Iniciando com 1
    while (true) {
        try {
            SocketSender sender(client_pid, socket_handle);
            if (sender.send_packet(payload, payload_size)) {
                //std::cout << "Open socket id found: " << int(socket_handle) << std::endl;
                return socket_handle;
            }
        }
        catch (...) {
            //Ignorar erros e continuar tentando
        }
        socket_handle = reinterpret_cast<HANDLE>(reinterpret_cast<uintptr_t>(socket_handle) + 1);
    }
}

std::vector<DWORD> GetPIDsByName(const std::wstring& processName) { //Função para encontrar todos os PIDs (Process ID) dos processos com o nome desejado
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create snapshot: " << GetLastError() << std::endl;
        return pids;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (processName == entry.szExeFile) {
                pids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snap, &entry));
    }
    else {
        std::cerr << "Failed to retrieve process information: " << GetLastError() << std::endl;
    }

    CloseHandle(snap);

    if (pids.empty()) {
        std::wcout << L"Process " << processName << L" not found." << std::endl;
        exit(1);
    }

    return pids;
}

DWORD SelectPID(const std::vector<DWORD>& pids) { //Função para selecionar o PID desejado
    if (pids.size() == 1) {
        return pids[0]; //Se houver apenas um processo, selecione-o automaticamente
    }

    std::wcout << L"Faldon clients found:" << std::endl;
    for (size_t i = 0; i < pids.size(); ++i) {
        std::wcout << i + 1 << L": PID " << pids[i] << std::endl;
    }

    int choice = 0;
    while (true) {
        std::wcout << L"Select a Faldon client according to its Process ID (1-" << pids.size() << L"): ";
        std::wcin >> choice;

        if (choice > 0 && choice <= static_cast<int>(pids.size())) {
            return pids[choice - 1];
        }

        std::wcout << L"Invalid selection. Please try again." << std::endl;
    }
}

int main() {
    std::wstring processName = L"Faldon.Client.exe"; //Substitua pelo nome do processo desejado
    std::vector<DWORD> pids = GetPIDsByName(processName);

    DWORD client_pid = SelectPID(pids);

    system("cls");

    unsigned char findSocketPacket[] = { 0x83, 0x01, 0x61, 0x61, 0x61 };
    HANDLE client_socket_handle = socketFinder(client_pid, findSocketPacket, sizeof(findSocketPacket));

    SocketSender sender(client_pid, client_socket_handle);

    system("cls");
    std::cout << "Packet Sender by clear-man" << std::endl;
    system("pause");

    return 0;
}