
#include <winsock2.h>
#include "imgui/imgui_impl_win32.h"
#include <gl/gl.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <detours.h>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <array>
#include <queue>   // Para a fila de monstros
#include <mutex>   // Para proteger a fila em multithreading
#include <deque>  // Alterar para deque
#include <conio.h>  // Para detectar teclas pressionadas
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include <vector>
#include <fstream>
#include <nfd.h>

#pragma comment(lib, "ws2_32.lib")  // Link Winsock library
#pragma comment(lib, "detours.lib")  // Link Detours library
#pragma comment(lib, "opengl32.lib")  // Link OpenGL library

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int ATK_SPEED = 500;
unsigned char targetPlayerID = 0x00;  // ID do player alvo
unsigned char playerID = 0x00;  // ID do seu player
unsigned char targetID_slow = 0x00;
unsigned char targetID_weaken = 0x00;
unsigned char targetID_curse = 0x00;
unsigned char targetID2_curse = 0x00;
unsigned char targetID3_curse = 0x00;
int counter = 0;
int death = 0;
int deathLimit = 0;
int counter_AutoPath_Egate = 1;
int counter_slow = 0;
int counter_weaken = 0;
int counter_curse1 = 0;
int counter_curse2 = 0;
int counter_curse3 = 0;
int counter_bless1 = 0;
const char* weapons[] = { "Select a weapon", "Master Sword", "Arctic Blade", "Skull Blade", "Caledfwlch", "Thor's Hammer", "Zeus' Fury", "Divine Sword", "Sword of Kings", "Mourningstar" };
int weaponsAttackSpeed[] = { 0, 400, 900, 1000, 950, 800, 800, 800, 800, 800 };
static int selectedWeapon = 0;
int teleportCounter = 0;
float Constitution = 0, Magic = 0;

std::deque<std::array<char, 5>> monsterQueue;  // Mudar de std::queue para std::deque
std::mutex queueMutex;  // Mutex para sincronizar o acesso à fila

std::deque<std::array<char, 4>> playerQueue;
std::mutex playerQueueMutex;

std::vector<std::array<int, 4>> newTeleportSpot;
std::vector<std::array<int, 4>> newAutoPathSpots;

bool PVP_PVE = true; // Global variable to control the program status
bool isAttacking = false;  // Variável que indica se estamos atacando um monstro no momento
bool AutoLoot = false;// Global variable to control the auto-loot status
bool AutoKill = false;
bool isPVPing = false;
bool AutoPK = false;
bool AutoBless = false;
bool AutoCure = false;
bool AutoHeal = false;
bool AutoAura = false;

bool isTeleporting = false;
bool AutoPath = false;

bool Enchanted = false;
bool SlowDebuff = false;

bool Spells = true;
bool HolyArmour = false, Antimagic = false, Empower = false, HolyStrike = false, Nova = false;

bool TheWatcher = false, BotFlag = false;

SOCKET socketHandle = INVALID_SOCKET;  // Socket handle for the connected server

// Function pointer for the original WSARecv function
typedef int (WINAPI* WSARecv_FUNC)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
WSARecv_FUNC originalWSARecv = nullptr;  // Pointer to the original WSARecv function

// Function pointer for the original WSASend function
typedef int (WINAPI* WSASend_FUNC)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
WSASend_FUNC originalWSASend = nullptr;  // Pointer to the original WSASend function

// Definição do tipo da função original wglSwapBuffers
typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC hdc);
wglSwapBuffers_t o_wglSwapBuffers = nullptr;  // Ponteiro para a função original

bool findSequenceInBuffer(const unsigned char* buffer, int bufferSize, const std::vector<unsigned char>& sequence) {
    for (int i = 0; i <= bufferSize - sequence.size(); ++i) {
        bool found = true;
        for (int j = 0; j < sequence.size(); ++j) {
            if (buffer[i + j] != sequence[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return true;
        }
    }
    return false;
}

void startAttack(SOCKET s) {
    std::thread([s]() {
        while (true) {
            if (!AutoKill || !PVP_PVE) {
                //std::cout << "Ataque pausado, aguardando..." << std::endl;
                if(monsterQueue.empty())
					break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Skip to the next iteration if paused
            }

            std::array<char, 5> attackPacket;
            bool continueAttacking = false;

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (!monsterQueue.empty()) {
                    attackPacket = monsterQueue.front();  // Pega o primeiro monstro da fila
                    continueAttacking = true;  // Continuar atacando enquanto houver monstro
                }
                else {
                    isAttacking = false;  // Se a fila estiver vazia, paramos de atacar
                    break;
                }
            }

            if (!continueAttacking) {
                //std::cout << "Empty queue." << std::endl;
                isAttacking = false;
                break;  // Sair da thread de ataque
            }

            WSABUF sendBuf;
            DWORD bytesSent = 0;
            sendBuf.len = 5;
            sendBuf.buf = const_cast<char*>(attackPacket.data());  // Pacote de ataque do monstro

            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(ATK_SPEED));

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (monsterQueue.empty()) {
                    isAttacking = false;
                   // std::cout << "Monster " << std::hex << (int)sendBuf.buf[2] << " " << (int)sendBuf.buf[3] << " removed from queue." << std::dec << std::endl;
                    break;
                }
            }
        }
        }).detach();
}

void startTeleport(SOCKET s) {
    std::thread([s]() {
        std::array<char, 8> teleport = { 0x86, 0x0F, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 };
        while (isTeleporting) {
            for (const auto& spot : newTeleportSpot) {
                int x = spot[0];
                int y = spot[1];
                int z = spot[2];
                int teleportDelay = spot[3];

                teleport[3] = static_cast<unsigned char>(x & 0xFF);
                teleport[4] = static_cast<unsigned char>((x >> 8) & 0xFF);

                teleport[5] = static_cast<unsigned char>(y & 0xFF);
                teleport[6] = static_cast<unsigned char>((y >> 8) & 0xFF);

                teleport[7] = static_cast<unsigned char>(z & 0xFF);

                WSABUF sendBuf;
                DWORD bytesSent = 0;
                sendBuf.len = 8;
                sendBuf.buf = const_cast<char*>(teleport.data());  // Pacote de teleport

                int sendTeleportPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                std::this_thread::sleep_for(std::chrono::milliseconds(teleportDelay));
            }
        }
        }).detach();
}

void startPVP(SOCKET s) {
    std::thread([s]() {
        while (true) {
            if (!AutoPK || !PVP_PVE) {
                //std::cout << "Ataque pausado, aguardando..." << std::endl;
                if (playerQueue.empty())
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Skip to the next iteration if paused
            }

            std::array<char, 4> attackPacket;
            bool continueAttacking = false;
            static bool autoKillWasOn = false;
            static bool wasTeleporting = false;

            {
                std::lock_guard<std::mutex> lock(playerQueueMutex);
                if (!playerQueue.empty()) {
                    attackPacket = playerQueue.front();  // Pega o primeiro player da fila
                    continueAttacking = true;  // Continuar atacando enquanto houver player
                    if (AutoKill) {
                        autoKillWasOn = true;
                        AutoKill = false;
                    }

                    if(isTeleporting){
						wasTeleporting = true;
						isTeleporting = false;
					}
                }
                else {
                    if (autoKillWasOn) {
                        AutoKill = true;
                        autoKillWasOn = false;
                    }

                    if(wasTeleporting){
                        isTeleporting = true;
                        wasTeleporting = false;
                        startTeleport(s);
                        }

                    isPVPing = false;  // Se a fila estiver vazia, paramos de atacar
                    break;
                }
            }

            if (!continueAttacking) {
                if (autoKillWasOn) {
                    AutoKill = true;
                    autoKillWasOn = false;
                }

                if(wasTeleporting){
					isTeleporting = true;
					wasTeleporting = false;
                    startTeleport(s);
				}
                
                isPVPing = false;
                //std::cout << "Empty queue." << std::endl;
                break;  // Sair da thread de ataque
            }

            WSABUF sendBuf;
            DWORD bytesSent = 0;
            sendBuf.len = 4;
            sendBuf.buf = const_cast<char*>(attackPacket.data());  // Pacote de ataque do player

            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
            counter++;
            //std::cout << "Sending attack packet: " << std::hex << (int)sendBuf.buf[0] << " " << (int)sendBuf.buf[1] << " " << (int)sendBuf.buf[2] << " " << (int)sendBuf.buf[3] << std::dec << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(ATK_SPEED));

            if (counter >= 20) {
                targetPlayerID = playerQueue.front()[2];

                playerQueue.pop_front();  // Remove o player
                counter = 0;
                //std::cout << "Player " << std::hex << (int)targetPlayerID << " timeout and removed from queue." << std::dec << std::endl;

                {
                    std::lock_guard<std::mutex> lock(playerQueueMutex);
                    // Reinicia o ataque ao pr�ximo Player, se a fila n�o estiver vazia
                    if (!playerQueue.empty()) {
                        //std::cout << "Attacking the next player in the queue: " << std::hex << (int)playerQueue.front()[2] << std::dec << std::endl;
                        targetPlayerID = playerQueue.front()[2];
                    }
                    else {
                        isPVPing = false;
                        targetPlayerID = 0x00;

                        if (autoKillWasOn) {
                            AutoKill = true;
                            autoKillWasOn = false;
                        }

                        if(wasTeleporting){
                            isTeleporting = true;
                            wasTeleporting = false;
                            startTeleport(s);
                        }

                        //std::cout << "Queue is empty." << std::endl;
                        break;
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(playerQueueMutex);
                if (playerQueue.empty()) {
                    if (autoKillWasOn) {
                        AutoKill = true;
                        autoKillWasOn = false;
                    }

                    if(wasTeleporting){
						isTeleporting = true;
						wasTeleporting = false;
                        startTeleport(s);
					}

                    isPVPing = false;
                    break;
                }
            }

        }
        }).detach();
}

void startAutoPath(SOCKET s) {
    std::thread([s]() {

        std::array<char, 8> teleport = { 0x86, 0x0F, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 };

            for (const auto& spot : newAutoPathSpots) {
                int x = spot[0];
                int y = spot[1];
                int z = spot[2];

                teleport[3] = static_cast<unsigned char>(x & 0xFF);
                teleport[4] = static_cast<unsigned char>((x >> 8) & 0xFF);

                teleport[5] = static_cast<unsigned char>(y & 0xFF);
                teleport[6] = static_cast<unsigned char>((y >> 8) & 0xFF);

                teleport[7] = static_cast<unsigned char>(z & 0xFF);

                WSABUF sendBuf;
                DWORD bytesSent = 0;
                sendBuf.len = 8;
                sendBuf.buf = const_cast<char*>(teleport.data());  // Pacote de teleport

                int sendTeleportPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }

            PVP_PVE = true;
            isTeleporting = true;
            startTeleport(s);
            Spells = true;

        }).detach();
}

void startHolyArmour(SOCKET s, int delay) {
    std::thread([s, delay]() {
        std::array<char, 6> HolyArmourPacket = { 0x84, 0x0F, 0x33, 0x01, playerID, 0x00 };

        while (HolyArmour && Spells || HolyArmour && !Spells) {
            WSABUF sendBuf;
            DWORD bytesSent = 0;
            sendBuf.len = 6;
            sendBuf.buf = const_cast<char*>(HolyArmourPacket.data());  // Pacote de HolyArmour

            int sendHolyArmourPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            while (HolyArmour && !Spells) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Skip to the next iteration if paused
            }
        }

        }).detach();
}

void startAntimagic(SOCKET s, int delay) {
    std::thread([s, delay]() {
        std::array<char, 6> AntimagicPacket = { 0x84, 0x0F, 0x41, 0x01, playerID, 0x00 };

        while (Antimagic && Spells || Antimagic && !Spells) {
            WSABUF sendBuf;
            DWORD bytesSent = 0;
            sendBuf.len = 6;
            sendBuf.buf = const_cast<char*>(AntimagicPacket.data());  // Pacote de Antimagic

            int sendAntimagicPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            while (Antimagic && !Spells) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Skip to the next iteration if paused
            }
        }

        }).detach();
}

void startNova(SOCKET s, int delay) {
    std::thread([s, delay]() {
        std::array<char, 3> NovaPacket = { 0x81, 0x0F, 0x25 };

        while (Nova && Spells || Nova && !Spells) {
            WSABUF sendBuf;
            DWORD bytesSent = 0;
            sendBuf.len = 3;
            sendBuf.buf = const_cast<char*>(NovaPacket.data());  // Pacote de Nova

            int sendNovaPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            while (Nova && !Spells) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Skip to the next iteration if paused
            }
        }

        }).detach();
}

void startHolyStrike(SOCKET s, int delay) {
    std::thread([s, delay]() {
        std::array<char, 3> HolyStrikePacket = { 0x81, 0x0F, 0x5E };

        while (HolyStrike && Spells || HolyStrike && !Spells) {
            WSABUF sendBuf;
            DWORD bytesSent = 0;
            sendBuf.len = 3;
            sendBuf.buf = const_cast<char*>(HolyStrikePacket.data());  // Pacote de HolyStrike

            int sendHolyStrikePacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            while (HolyStrike && !Spells) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Skip to the next iteration if paused
            }
        }

        }).detach();
}

void startEmpower(SOCKET s, int delay) {
    std::thread([s, delay]() {
        std::array<char, 6> EmpowerPacket = { 0x84, 0x0F, 0x45, 0x01, playerID, 0x00 };

        while (Empower && Spells || Empower && !Spells) {
            WSABUF sendBuf;
            DWORD bytesSent = 0;
            sendBuf.len = 6;
            sendBuf.buf = const_cast<char*>(EmpowerPacket.data());  // Pacote de Empower

            int sendEmpowerPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            while (Empower && !Spells) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;  // Skip to the next iteration if paused
            }
        }

        }).detach();
}

// Função HookedWSARecv para detectar e remover o monstro morto corretamente
int WINAPI HookedWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    socketHandle = s;  // Save the socket handle for later use

    // Call the original WSARecv function
    int result = originalWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);

    // Check if data was successfully received
    if (result == 0 && lpNumberOfBytesRecvd && *lpNumberOfBytesRecvd > 0) {
        //std::cout << "New packet received." << std::endl;

        for (DWORD i = 0; i < dwBufferCount; ++i) {
            WSABUF buffer = lpBuffers[i];
            DWORD bytesReceived = *lpNumberOfBytesRecvd;

            // Iterate over the received data and search for the sequence 83 A2
            for (DWORD j = 0; j < bytesReceived; j++) {
                //std::cout << "New byte read." << std::endl;

                if (PVP_PVE) {

                    if (playerID == 0x00) {
                        if ((unsigned char)buffer.buf[j] == 0x8a && (unsigned char)buffer.buf[j + 1] == 0x17) {
                            playerID = buffer.buf[j + 2];
                        }
                    }

                    if ((unsigned char)buffer.buf[j] == 0x83 && (unsigned char)buffer.buf[j + 1] == 0xa2 && AutoKill) {
                        // Monstro detectado (83 A2 byte2 byte1)
                        unsigned char byte1 = buffer.buf[j + 2];
                        unsigned char byte2 = buffer.buf[j + 3];

                        //std::cout << "Monster found: " << std::hex << (int)byte2 << " " << (int)byte1 << std::dec << std::endl;

                        // Cria o pacote de ataque
                        std::array<char, 5> attackPacket = { 0x83, 0x0A, byte2, byte1, 0xFF };

                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            // Adiciona o monstro no in�cio da fila
                            monsterQueue.push_front(attackPacket);
                        }

                        // Inicia o ataque se n�o estiver atacando ningu�m
                        if (!isAttacking && !monsterQueue.empty()) {
                            isAttacking = true;
                            startAttack(s);  // Inicia o ataque em uma nova thread
                        }
                    }

                    if ((unsigned char)buffer.buf[j] == 0x82 && (unsigned char)buffer.buf[j + 1] == 0xa2 && AutoKill) {
                        // Monstro detectado (82 A2 byte1)
                        unsigned char byte1 = buffer.buf[j + 2] - 0x80;

                        //std::cout << "Monster found: " << std::hex << (int)byte1 << std::dec << std::endl;

                        // Cria o pacote de ataque
                        std::array<char, 5> attackPacket = { 0x83, 0x0A, byte1, 0x00, 0xFF };

                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            // Adiciona o monstro no in�cio da fila
                            monsterQueue.push_front(attackPacket);
                        }

                        // Inicia o ataque se n�o estiver atacando ningu�m
                        if (!isAttacking && !monsterQueue.empty()) {
                            isAttacking = true;
                            startAttack(s);  // Inicia o ataque em uma nova thread
                        }

                    }


                    if ((unsigned char)buffer.buf[j] == 0x84 && (unsigned char)buffer.buf[j + 1] == 0x5F && AutoKill) {
                        // Ataque de monstro detectado (84 5F byte2 byte1)
                        unsigned char byte1 = buffer.buf[j + 3];
                        unsigned char byte2 = buffer.buf[j + 2];

                        unsigned char attackByte1 = 0x00;
                        unsigned char attackByte2 = 0x00;

                        unsigned char secondDigitByte1 = byte1 & 0x0F;

                        if (secondDigitByte1 == 0x0 || secondDigitByte1 == 0x1 || secondDigitByte1 == 0x4 || secondDigitByte1 == 0x5 || secondDigitByte1 == 0x8 || secondDigitByte1 == 0x9 || secondDigitByte1 == 0xC || secondDigitByte1 == 0xD) {
                            attackByte1 = byte1 + 0x2;
                        }
                        else if (secondDigitByte1 == 0x2 || secondDigitByte1 == 0x3 || secondDigitByte1 == 0x6 || secondDigitByte1 == 0x7 || secondDigitByte1 == 0xA || secondDigitByte1 == 0xB || secondDigitByte1 == 0xE || secondDigitByte1 == 0xF) {
                            attackByte1 = byte1 - 0x2;
                        }

                        if (byte2 == 0x00)
                            attackByte2 = 0x01;
                        else if (byte2 == 0x01)
                            attackByte2 = 0x00;
                        else if (byte2 == 0x02)
                            attackByte2 = 0x03;
                        else if (byte2 == 0x03)
                            attackByte2 = 0x02;

                        std::array<char, 5> attackPacket = { 0x83, 0x0A, attackByte1, attackByte2, 0xFF };

                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            // Adiciona o monstro no in�cio da fila
                            monsterQueue.push_front(attackPacket);
                        }

                        // Inicia o ataque se n�o estiver atacando ningu�m
                        if (!isAttacking && !monsterQueue.empty()) {
                            isAttacking = true;
                            startAttack(s);  // Inicia o ataque em uma nova thread
                        }

                    }

                    if ((unsigned char)buffer.buf[j] == 0x83 && (unsigned char)buffer.buf[j + 1] == 0x5F && AutoKill) {
                        // Ataque de monstro detectado (83 5F byte1)
                        unsigned char byte1 = buffer.buf[j + 2] - 0x80;
                        unsigned char byte2 = 0x00;

                        unsigned char attackByte1 = 0x00;
                        unsigned char attackByte2 = byte2;

                        unsigned char secondDigitByte1 = byte1 & 0x0F;

                        if (secondDigitByte1 == 0x0 || secondDigitByte1 == 0x2 || secondDigitByte1 == 0x4 || secondDigitByte1 == 0x6 || secondDigitByte1 == 0x8 || secondDigitByte1 == 0xA || secondDigitByte1 == 0xC || secondDigitByte1 == 0xE) {
                            attackByte1 = byte1 + 0x1;
                        }
                        else if (secondDigitByte1 == 0x1 || secondDigitByte1 == 0x3 || secondDigitByte1 == 0x5 || secondDigitByte1 == 0x7 || secondDigitByte1 == 0x9 || secondDigitByte1 == 0xB || secondDigitByte1 == 0xD || secondDigitByte1 == 0xF) {
                            attackByte1 = byte1 - 0x1;
                        }

                        std::array<char, 5> attackPacket = { 0x83, 0x0A, attackByte1, attackByte2, 0xFF };


                        {
                            std::lock_guard<std::mutex> lock(queueMutex);
                            // Adiciona o monstro no in�cio da fila
                            monsterQueue.push_front(attackPacket);
                        }

                        // Inicia o ataque se n�o estiver atacando ningu�m
                        if (!isAttacking && !monsterQueue.empty()) {
                            isAttacking = true;
                            startAttack(s);  // Inicia o ataque em uma nova thread
                        }

                    }

                    if (((unsigned char)buffer.buf[j] == 0x89 || (unsigned char)buffer.buf[j] == 0x8A || (unsigned char)buffer.buf[j] == 0x8B || (unsigned char)buffer.buf[j] == 0x8C || (unsigned char)buffer.buf[j] == 0x8D || (unsigned char)buffer.buf[j] == 0x8E || (unsigned char)buffer.buf[j] == 0x8F) && (unsigned char)buffer.buf[j + 1] == 0x8E && (unsigned char)buffer.buf[j + 2] > 0x80 && AutoLoot) {
                        // Sequence found: extract the next two bytes
                        // Extrai o byte em buffer.buf[j + 2] para criar byte1
                        unsigned char byte1 = buffer.buf[j + 2];

                        // Calcula o primeiro d�gito de byte1: subtraindo 0x8 do d�gito mais significativo
                        unsigned char firstDigit = (byte1 >> 4) - 0x8;
                        if (firstDigit > 0xF) firstDigit = 0;  // Verifica��o para evitar underflow

                        // Mant�m o segundo d�gito de byte1
                        unsigned char secondDigit = byte1 & 0x0F;

                        // Recria byte1 com o d�gito mais significativo ajustado
                        byte1 = (firstDigit << 4) | secondDigit;

                        // Extrai o byte em buffer.buf[j + 3] para byte2
                        unsigned char byte2 = buffer.buf[j + 3];

                        // Exibe os valores calculados
                        //std::cout << "Item found. byte1: " << std::hex << (int)buffer.buf[j + 5] << " byte2: " << (int)buffer.buf[j + 6] << std::dec << std::endl;

                        if (!(((unsigned char)buffer.buf[j + 5] == 0xCD) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0xA0) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x16) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x61) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x60) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xCE) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0x95) || ((unsigned char)buffer.buf[j + 5] == 0xAF) || ((unsigned char)buffer.buf[j + 5] == 0x89) || ((unsigned char)buffer.buf[j + 5] == 0x86) || ((unsigned char)buffer.buf[j + 5] == 0xFF) || ((unsigned char)buffer.buf[j + 5] == 0xA4) || ((unsigned char)buffer.buf[j + 5] == 0x9C) || ((unsigned char)buffer.buf[j + 5] == 0x92) || ((unsigned char)buffer.buf[j + 5] == 0x9F) || ((unsigned char)buffer.buf[j + 5] == 0xFA) || ((unsigned char)buffer.buf[j + 5] == 0xF0) || ((unsigned char)buffer.buf[j + 5] == 0xB5) || ((unsigned char)buffer.buf[j + 5] == 0x9D) || ((unsigned char)buffer.buf[j + 5] == 0x99) || ((unsigned char)buffer.buf[j + 5] == 0x9E) || ((unsigned char)buffer.buf[j + 5] == 0xA0) || ((unsigned char)buffer.buf[j + 5] == 0x95) || ((unsigned char)buffer.buf[j + 5] == 0x9A) || ((unsigned char)buffer.buf[j + 5] == 0xAD) || ((unsigned char)buffer.buf[j + 5] == 0x9B) || ((unsigned char)buffer.buf[j + 5] == 0x87) || ((unsigned char)buffer.buf[j + 5] == 0x83) || ((unsigned char)buffer.buf[j + 5] == 0x84) || ((unsigned char)buffer.buf[j + 5] == 0x81) || ((unsigned char)buffer.buf[j + 5] == 0x8E) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x4C) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0xB0) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x66) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x52) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xBE) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0x9C) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0xAD) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xE8) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x81) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x56) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xBF) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0x8C) || ((unsigned char)buffer.buf[j + 5] == 0x82) || ((unsigned char)buffer.buf[j + 5] == 0x8B) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x3B) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x3C) || ((unsigned char)buffer.buf[j + 5] == 0x8C) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x2C) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x2F) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x60) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xC0) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0x97) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xFC) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0x8B) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x0D) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x00) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x01) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xD1) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x4F) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xCD) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xCF) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x02) || ((unsigned char)buffer.buf[j + 5] == 0x00 && (unsigned char)buffer.buf[j + 6] == 0xD2) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x61) || ((unsigned char)buffer.buf[j + 5] == 0x8A) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0x5B) || ((unsigned char)buffer.buf[j + 5] == 0x01 && (unsigned char)buffer.buf[j + 6] == 0xA9))) {
                            //std::cout << "You picked up the item." << std::endl;

                            // Construct the loot packet: 83 0B 7A byte2 byte1
                            char lootPacket[5];
                            lootPacket[0] = 0x83;
                            lootPacket[1] = 0x0B;
                            lootPacket[2] = 0x7A;
                            lootPacket[3] = byte1;
                            lootPacket[4] = byte2;

                            // Construct the telekinesis packet: 83 10 54 byte2 byte1
                            char telePacket[5];
                            telePacket[0] = 0x83;
                            telePacket[1] = 0x10;
                            telePacket[2] = 0x54;
                            telePacket[3] = byte1;
                            telePacket[4] = byte2;

                            WSABUF sendBuf;
                            DWORD bytesSent = 0;
                            sendBuf.len = 5;

                            if ((unsigned char)buffer.buf[j + 6] != 0xD5) {
                                // Send the telekinesis packet back to the server
                                sendBuf.buf = telePacket;
                                int sendTelePacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            }

                            // Send the loot packet back to the server
                            sendBuf.buf = lootPacket;
                            bytesSent = 0;
                            int sendLootPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                        }
                        //else
                            //std::cout << "Filtered item." << std::endl;
                    }

                    if ((((unsigned char)buffer.buf[j] == 0x89 || (unsigned char)buffer.buf[j] == 0x8A || (unsigned char)buffer.buf[j] == 0x8B || (unsigned char)buffer.buf[j] == 0x8C || (unsigned char)buffer.buf[j] == 0x8D || (unsigned char)buffer.buf[j] == 0x8E || (unsigned char)buffer.buf[j] == 0x8F) && (unsigned char)buffer.buf[j + 1] == 0x8E && (unsigned char)buffer.buf[j + 2] == 0x00) && AutoLoot) {
                        // Sequence found: extract the next two bytes
                        // Extrai o byte em buffer.buf[j + 2] para criar byte1
                        unsigned char byte1 = buffer.buf[j + 3];

                        // Extrai o byte em buffer.buf[j + 3] para byte2
                        unsigned char byte2 = buffer.buf[j + 4];

                        // Exibe os valores calculados
                        //std::cout << "Item found. byte1: " << std::hex << (int)buffer.buf[j + 6] << " byte2: " << (int)buffer.buf[j + 7] << std::dec << std::endl;

                        if (!(((unsigned char)buffer.buf[j + 6] == 0xCD) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xA0) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x16) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x61) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x60) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xCE) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x95) || ((unsigned char)buffer.buf[j + 6] == 0xAF) || ((unsigned char)buffer.buf[j + 6] == 0x89) || ((unsigned char)buffer.buf[j + 6] == 0x86) || ((unsigned char)buffer.buf[j + 6] == 0xFF) || ((unsigned char)buffer.buf[j + 6] == 0xA4) || ((unsigned char)buffer.buf[j + 6] == 0x9C) || ((unsigned char)buffer.buf[j + 6] == 0x92) || ((unsigned char)buffer.buf[j + 6] == 0x9F) || ((unsigned char)buffer.buf[j + 6] == 0xFA) || ((unsigned char)buffer.buf[j + 6] == 0xF0) || ((unsigned char)buffer.buf[j + 6] == 0xB5) || ((unsigned char)buffer.buf[j + 6] == 0x9D) || ((unsigned char)buffer.buf[j + 6] == 0x99) || ((unsigned char)buffer.buf[j + 6] == 0x9E) || ((unsigned char)buffer.buf[j + 6] == 0xA0) || ((unsigned char)buffer.buf[j + 6] == 0x95) || ((unsigned char)buffer.buf[j + 6] == 0x9A) || ((unsigned char)buffer.buf[j + 6] == 0xAD) || ((unsigned char)buffer.buf[j + 6] == 0x9B) || ((unsigned char)buffer.buf[j + 6] == 0x87) || ((unsigned char)buffer.buf[j + 6] == 0x83) || ((unsigned char)buffer.buf[j + 6] == 0x84) || ((unsigned char)buffer.buf[j + 6] == 0x81) || ((unsigned char)buffer.buf[j + 6] == 0x8E) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x4C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xB0) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x66) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x52) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xBE) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x9C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xAD) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xE8) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x81) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x56) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xBF) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x8C) || ((unsigned char)buffer.buf[j + 6] == 0x82) || ((unsigned char)buffer.buf[j + 6] == 0x8B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x3B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x3C) || ((unsigned char)buffer.buf[j + 6] == 0x8C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x2C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x2F) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x60) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xC0) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x97) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xFC) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x8B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x0D) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x00) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x01) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xD1) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x4F) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xCD) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xCF) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x02) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xD2) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x61) || ((unsigned char)buffer.buf[j + 6] == 0x8A) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x5B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xA9))) {
                            //std::cout << "You picked up the item." << std::endl;

                            // Construct the loot packet: 83 0B 7A byte2 byte1
                            char lootPacket[5];
                            lootPacket[0] = 0x83;
                            lootPacket[1] = 0x0B;
                            lootPacket[2] = 0x7A;
                            lootPacket[3] = byte1;
                            lootPacket[4] = byte2;

                            // Construct the telekinesis packet: 83 10 54 byte2 byte1
                            char telePacket[5];
                            telePacket[0] = 0x83;
                            telePacket[1] = 0x10;
                            telePacket[2] = 0x54;
                            telePacket[3] = byte1;
                            telePacket[4] = byte2;

                            // Send the telekinesis packet back to the server
                            WSABUF sendBuf;
                            DWORD bytesSent = 0;
                            sendBuf.len = 5;

                            if ((unsigned char)buffer.buf[j + 7] != 0xD5) {
                                // Send the telekinesis packet back to the server
                                sendBuf.buf = telePacket;
                                int sendTelePacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            }

                            // Send the loot packet back to the server
                            sendBuf.buf = lootPacket;
                            bytesSent = 0;
                            int sendLootPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                        }
                        //else
                            //std::cout << "Filtered item." << std::endl;
                    }

                    if (((unsigned char)buffer.buf[j] == 0x89 || (unsigned char)buffer.buf[j] == 0x8A || (unsigned char)buffer.buf[j] == 0x8B || (unsigned char)buffer.buf[j] == 0x8C || (unsigned char)buffer.buf[j] == 0x8D || (unsigned char)buffer.buf[j] == 0x8E || (unsigned char)buffer.buf[j] == 0x8F) && (unsigned char)buffer.buf[j + 1] == 0x8E && ((unsigned char)buffer.buf[j + 2] >= 0x01 && (unsigned char)buffer.buf[j + 2] <= 0x80) && AutoLoot) {
                        // Sequence found: extract the next two bytes
                        // Extrai o byte em buffer.buf[j + 2] para criar byte1
                        unsigned char byte1 = buffer.buf[j + 3];

                        // Extrai o byte em buffer.buf[j + 3] para byte2
                        unsigned char byte2 = buffer.buf[j + 2];

                        // Exibe os valores calculados
                        //std::cout << "Item found. byte1: " << std::hex << (int)buffer.buf[j + 6] << " byte2: " << (int)buffer.buf[j + 7] << std::dec << std::endl;

                        if (!(((unsigned char)buffer.buf[j + 6] == 0xCD) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xA0) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x16) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x61) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x60) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xCE) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x95) || ((unsigned char)buffer.buf[j + 6] == 0xAF) || ((unsigned char)buffer.buf[j + 6] == 0x89) || ((unsigned char)buffer.buf[j + 6] == 0x86) || ((unsigned char)buffer.buf[j + 6] == 0xFF) || ((unsigned char)buffer.buf[j + 6] == 0xA4) || ((unsigned char)buffer.buf[j + 6] == 0x9C) || ((unsigned char)buffer.buf[j + 6] == 0x92) || ((unsigned char)buffer.buf[j + 6] == 0x9F) || ((unsigned char)buffer.buf[j + 6] == 0xFA) || ((unsigned char)buffer.buf[j + 6] == 0xF0) || ((unsigned char)buffer.buf[j + 6] == 0xB5) || ((unsigned char)buffer.buf[j + 6] == 0x9D) || ((unsigned char)buffer.buf[j + 6] == 0x99) || ((unsigned char)buffer.buf[j + 6] == 0x9E) || ((unsigned char)buffer.buf[j + 6] == 0xA0) || ((unsigned char)buffer.buf[j + 6] == 0x95) || ((unsigned char)buffer.buf[j + 6] == 0x9A) || ((unsigned char)buffer.buf[j + 6] == 0xAD) || ((unsigned char)buffer.buf[j + 6] == 0x9B) || ((unsigned char)buffer.buf[j + 6] == 0x87) || ((unsigned char)buffer.buf[j + 6] == 0x83) || ((unsigned char)buffer.buf[j + 6] == 0x84) || ((unsigned char)buffer.buf[j + 6] == 0x81) || ((unsigned char)buffer.buf[j + 6] == 0x8E) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x4C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xB0) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x66) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x52) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xBE) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x9C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xAD) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xE8) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x81) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x56) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xBF) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x8C) || ((unsigned char)buffer.buf[j + 6] == 0x82) || ((unsigned char)buffer.buf[j + 6] == 0x8B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x3B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x3C) || ((unsigned char)buffer.buf[j + 6] == 0x8C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x2C) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x2F) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x60) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xC0) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x97) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xFC) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0x8B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x0D) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x00) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x01) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xD1) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x4F) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xCD) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xCF) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x02) || ((unsigned char)buffer.buf[j + 6] == 0x00 && (unsigned char)buffer.buf[j + 7] == 0xD2) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x61) || ((unsigned char)buffer.buf[j + 6] == 0x8A) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0x5B) || ((unsigned char)buffer.buf[j + 6] == 0x01 && (unsigned char)buffer.buf[j + 7] == 0xA9))) {
                            //std::cout << "You picked up the item." << std::endl;

                            // Construct the loot packet: 83 0B 7A byte2 byte1
                            char lootPacket[5];
                            lootPacket[0] = 0x83;
                            lootPacket[1] = 0x0B;
                            lootPacket[2] = 0x7A;
                            lootPacket[3] = byte1;
                            lootPacket[4] = byte2;

                            // Construct the telekinesis packet: 83 10 54 byte2 byte1
                            char telePacket[5];
                            telePacket[0] = 0x83;
                            telePacket[1] = 0x10;
                            telePacket[2] = 0x54;
                            telePacket[3] = byte1;
                            telePacket[4] = byte2;

                            // Send the telekinesis packet back to the server
                            WSABUF sendBuf;
                            DWORD bytesSent = 0;
                            sendBuf.len = 5;

                            if ((unsigned char)buffer.buf[j + 7] != 0xD5) {
                                // Send the telekinesis packet back to the server
                                sendBuf.buf = telePacket;
                                int sendTelePacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            }

                            // Send the loot packet back to the server
                            sendBuf.buf = lootPacket;
                            bytesSent = 0;
                            int sendLootPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                        }
                        //else
                            //std::cout << "Filtered item." << std::endl;
                    }

                    if ((unsigned char)buffer.buf[j] == 0x83 && (unsigned char)buffer.buf[j + 1] == 0x2C && (unsigned char)buffer.buf[j + 2] != targetPlayerID && AutoPK) {
                        unsigned char ID = playerID + 0x80;
                        bool sequenceFound = false;
                        for (DWORD k = j + 3; k < j + 3 + 55 && k < bytesReceived; k++) { //
                            if ((unsigned char)buffer.buf[k] == 0x82 &&
                                (unsigned char)buffer.buf[k + 1] == 0xA0 &&
                                (unsigned char)buffer.buf[k + 2] == ID) {
                                sequenceFound = true;
                                break;
                            }
                        }

                        if (sequenceFound) {

                            // Player detectado (83 2C targetPlayerID)
                            targetPlayerID = buffer.buf[j + 2];

                            //std::cout << "Player detected: " << std::hex << (int)targetPlayerID << std::dec << std::endl;

                            // Cria o pacote de ataque
                            std::array<char, 4> attackPacket = { 0x82, 0x08, targetPlayerID, 0xFF };

                            {
                                std::lock_guard<std::mutex> lock(playerQueueMutex);
                                // Adiciona o Player no in�cio da fila
                                counter = 0;
                                playerQueue.push_front(attackPacket);
                            }

                            // Inicia o ataque se n�o estiver atacando ningu�m
                            if (!isPVPing) {
                                isPVPing = true;
                                startPVP(s);  // Inicia o ataque em uma nova thread
                            }
                        }
                    }

                    // Verifica se � o pacote de morte do Player (82 0C targetPlayerID)
                    if ((unsigned char)buffer.buf[j] == 0x82 && (unsigned char)buffer.buf[j + 1] == 0x0C && (unsigned char)buffer.buf[j + 2] == targetPlayerID && AutoPK) {
                        {
                            std::lock_guard<std::mutex> lock(playerQueueMutex);
                            // Verifica e remove o player morto corretamente
                            if (!playerQueue.empty()) {
                                //std::cout << "Verificando player morto na fila." << std::endl;

                                targetPlayerID = playerQueue.front()[2];

                                playerQueue.pop_front();  // Remove o player morto
                                //std::cout << "Player " << std::hex << (int)targetPlayerID << " died and removed from queue." << std::dec << std::endl;

                                targetPlayerID = 0x00;  // Reseta o ID do player

                            }

                            // Reinicia o ataque ao pr�ximo Player, se a fila n�o estiver vazia
                            if (!playerQueue.empty()) {
                                //std::cout << "Attacking the next player in the queue: " << std::hex << (int)playerQueue.front()[2] << std::dec << std::endl;
                                targetPlayerID = playerQueue.front()[2];
                            }
                            else {
                                isPVPing = false;
                                targetPlayerID = 0x00;
                                //std::cout << "Queue is empty." << std::endl;
                            }
                        }
                    }

                    //8e 17 2a for Slowed action / 8a 17 2a for Curse / 8b 17 2a for Weaken
                    if ((((unsigned char)buffer.buf[j] == 0x8e && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] != playerID && (unsigned char)buffer.buf[j + 4] == 0x58 && (unsigned char)buffer.buf[j + 19] == targetID_slow) || ((unsigned char)buffer.buf[j] == 0x8e && (unsigned char)buffer.buf[j + 1] == 0xc7 && (unsigned char)buffer.buf[j + 4] == 0x5b) || ((unsigned char)buffer.buf[j] == 0x8a && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] != playerID && (unsigned char)buffer.buf[j + 4] == 0x47 && ((unsigned char)buffer.buf[j + 15] == targetID_curse || (unsigned char)buffer.buf[j + 15] == targetID2_curse || (unsigned char)buffer.buf[j + 15] == targetID3_curse)) || ((unsigned char)buffer.buf[j] == 0x8b && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] != playerID && (unsigned char)buffer.buf[j + 4] == 0x57 && (unsigned char)buffer.buf[j + 16] == targetID_weaken)) && AutoBless) {
                        std::array<char, 6> blessPacket = { 0x84, 0x0F, 0x0E, 0x01, playerID, 0x00 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 6;
                        sendBuf.buf = const_cast<char*>(blessPacket.data());  // Pacote de ataque do monstro

                        int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                    }

                    if (targetID_curse == 0x00 && playerID != 0x00) {
                        std::array<char, 6> Curse = { 0x84, 0x0F, 0x58, 0x01, playerID, 0x00 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 6;
                        sendBuf.buf = const_cast<char*>(Curse.data());  // Pacote de ataque do monstro

                        if (counter_curse1 == 0) {
                            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            Sleep(500);
                            counter_curse1++;
                        }

                        if ((unsigned char)buffer.buf[j] == 0x8A && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] == playerID && (unsigned char)buffer.buf[j + 4] == 0x47 && (unsigned char)buffer.buf[j + 5] == 0x6F && (unsigned char)buffer.buf[j + 6] == 0x6E) {
                            targetID_curse = buffer.buf[j + 15];
                        }
                    }

                    if (targetID_slow == 0x00 && playerID != 0x00) {
                        std::array<char, 6> SlowedAction = { 0x84, 0x0F, 0x2E, 0x01, playerID, 0x00 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 6;
                        sendBuf.buf = const_cast<char*>(SlowedAction.data());  // Pacote de ataque do monstro

                        if (counter_slow == 0) {
                            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            Sleep(500);
                            counter_slow++;
                        }

                        if ((unsigned char)buffer.buf[j] == 0x8E && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] == playerID) {
                            targetID_slow = buffer.buf[j + 19];
                        }

                    }

                    if (targetID_weaken == 0x00 && playerID != 0x00) {
                        std::array<char, 6> Weaken = { 0x84, 0x0F, 0x2A, 0x01, playerID, 0x00 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 6;
                        sendBuf.buf = const_cast<char*>(Weaken.data());  // Pacote de ataque do monstro

                        if (counter_weaken == 0) {
                            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            Sleep(500);
                            counter_weaken++;
                        }

                        if ((unsigned char)buffer.buf[j] == 0x8B && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] == playerID) {
                            targetID_weaken = buffer.buf[j + 16];
                        }
                    }

                    if (targetID2_curse == 0x00 && playerID != 0x00) {
                        std::array<char, 6> Curse = { 0x84, 0x0F, 0x58, 0x01, playerID, 0x00 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 6;
                        sendBuf.buf = const_cast<char*>(Curse.data());

                        if (counter_curse2 == 0) {
                            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            Sleep(500);
                            counter_curse2++;
                        }

                        if ((unsigned char)buffer.buf[j] == 0x8A && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] == playerID && (unsigned char)buffer.buf[j + 4] == 0x47 && (unsigned char)buffer.buf[j + 5] == 0x6F && (unsigned char)buffer.buf[j + 6] == 0x6E && (unsigned char)buffer.buf[j + 15] != targetID_curse) {
                            targetID2_curse = buffer.buf[j + 15];
                        }
                    }

                    if (targetID3_curse == 0x00 && playerID != 0x00) {
                        std::array<char, 6> blessPacket = { 0x84, 0x0F, 0x0E, 0x01, playerID, 0x00 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 6;
                        sendBuf.buf = const_cast<char*>(blessPacket.data());

                        if (counter_bless1 == 0) {
                            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            Sleep(500);
                            counter_bless1++;
                        }

                        std::array<char, 6> Curse = { 0x84, 0x0F, 0x58, 0x01, playerID, 0x00 };

                        bytesSent = 0;
                        sendBuf.len = 6;
                        sendBuf.buf = const_cast<char*>(Curse.data());

                        if (counter_curse3 == 0) {
                            int sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                            Sleep(500);
                            counter_curse3++;

                            bytesSent = 0;
                            sendBuf.buf = const_cast<char*>(blessPacket.data());
                            sendAttackPacketResult = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                        }

                        if ((unsigned char)buffer.buf[j] == 0x8A && (unsigned char)buffer.buf[j + 1] == 0x17 && (unsigned char)buffer.buf[j + 2] == playerID && (unsigned char)buffer.buf[j + 4] == 0x47 && (unsigned char)buffer.buf[j + 5] == 0x6F && (unsigned char)buffer.buf[j + 6] == 0x6E && (unsigned char)buffer.buf[j + 15] != targetID_curse && (unsigned char)buffer.buf[j + 15] != targetID2_curse) {
                            targetID3_curse = buffer.buf[j + 15];
                        }
                    }

                }

                if (AutoPath) {
                    if ((unsigned char)buffer.buf[j] == 0x41 && (unsigned char)buffer.buf[j + 1] == 0x64 && (unsigned char)buffer.buf[j + 2] == 0x05 && (unsigned char)buffer.buf[j + 3] == 0x00 && (unsigned char)buffer.buf[j + 4] == 0x73) {
                        //Morte detectada
                        unsigned char ID = playerID + 0x80;
                        std::vector<unsigned char> sequence = { 0x82, 0xa0, ID };

                        if (findSequenceInBuffer((unsigned char*)buffer.buf, *lpNumberOfBytesRecvd, sequence)) {
                            PVP_PVE = false, isTeleporting = false, Spells = false;
                            std::array<char, 3> respawn = { 0x81, 0x47, 0x00 };

                            WSABUF sendBuf;
                            DWORD bytesSent = 0;
                            sendBuf.len = 3;
                            sendBuf.buf = const_cast<char*>(respawn.data());

                            Sleep(500);
                            int sendRespawnPacket = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);

                            death++;

                            counter_AutoPath_Egate = 0;

                            std::array<char, 3> EGate = { 0x81, 0x0F, 0x39 };

                            bytesSent = 0;
                            sendBuf.len = 3;
                            sendBuf.buf = const_cast<char*>(EGate.data());

                            Sleep(500);
                            int sendEGatePacket = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);
                        }
                    }

                    if ((unsigned char)buffer.buf[j] == 0x86 && (unsigned char)buffer.buf[j + 1] == 0x8D && counter_AutoPath_Egate == 0) {
                        //Detecta EGATE para egate id menor que 128
                        unsigned char byte1 = (unsigned char)buffer.buf[j + 2] - 0x80;

                        std::array<char, 5> enterEGate = { 0x83, 0x0b, 0x7a, byte1, 0x00 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 5;
                        sendBuf.buf = const_cast<char*>(enterEGate.data());

                        Sleep(500);
                        int sendEnterEGatePacket = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);

                        counter_AutoPath_Egate = 1;

                        std::array<char, 3> EGate = { 0x81, 0x0F, 0x39 };

                        bytesSent = 0;
                        sendBuf.len = 3;
                        sendBuf.buf = const_cast<char*>(EGate.data());

                        Sleep(500);
                        int sendEGatePacket = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);

                        Sleep(500);
                        startAutoPath(s);
                    }

                    if ((unsigned char)buffer.buf[j] == 0x87 && (unsigned char)buffer.buf[j + 1] == 0x8D && counter_AutoPath_Egate == 0) {
                        //Detecta EGATE para egate id maior que 128
                        unsigned char byte1 = (unsigned char)buffer.buf[j + 3];
                        unsigned char byte2 = (unsigned char)buffer.buf[j + 2];

                        std::array<char, 5> enterEGate = { 0x83, 0x0b, 0x7a, byte1, byte2 };

                        WSABUF sendBuf;
                        DWORD bytesSent = 0;
                        sendBuf.len = 5;
                        sendBuf.buf = const_cast<char*>(enterEGate.data());

                        Sleep(500);
                        int sendEnterEGatePacket = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);

                        counter_AutoPath_Egate = 1;

                        std::array<char, 3> EGate = { 0x81, 0x0F, 0x39 };

                        bytesSent = 0;
                        sendBuf.len = 3;
                        sendBuf.buf = const_cast<char*>(EGate.data());

                        Sleep(500);
                        int sendEGatePacket = originalWSASend(s, &sendBuf, 1, &bytesSent, 0, NULL, NULL);

                        Sleep(500);
                        startAutoPath(s);
                    }

                }

            }

        }
    }

    else if (result != 0) {
        std::cerr << "WSARecv falhou com erro: " << WSAGetLastError() << std::endl;
    }

    return result;
}

// Hooked WSASend function (optional, can be used if necessary)
int WINAPI HookedWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    return originalWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}

HWND hGameWindow;
WNDPROC hGameWindowProc;
bool menuShown = true;

// Hook for the window procedure to handle inputs
LRESULT CALLBACK windowProc_hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Toggle the overlay using the delete key
    if (uMsg == WM_KEYDOWN && wParam == VK_DELETE) {
        menuShown = !menuShown;
        return 0;
    }

    // Let ImGui decide if it should capture this input
    if (menuShown && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;  // Se ImGui tratar o evento, retornamos true
    }

    // Otherwise, call the original game's window procedure
    return CallWindowProc(hGameWindowProc, hWnd, uMsg, wParam, lParam);
}

void loadTeleportsFromFile(std::vector<std::array<int, 4>>& teleportSpots, const char* filePath) {
    std::ifstream file(filePath);

    std::array<int, 4> spot;
    while (file >> spot[0] >> spot[1] >> spot[2] >> spot[3]) {
        teleportSpots.push_back(spot);
    }

    file.close();
}

void saveTeleportsToFile(const std::vector<std::array<int, 4>>& teleportSpots, const char* filePath) {
    std::ofstream file(filePath);
    if (file.is_open()) {
        for (const auto& spot : teleportSpots) {
            file << spot[0] << " " << spot[1] << " " << spot[2] << " " << spot[3] << std::endl;
        }
    }

    file.close();
}

// Hooked wglSwapBuffers function
BOOL WINAPI Hooked_wglSwapBuffers(HDC hdc) {
    // Initialize ImGui and OpenGL, but only once
    static bool imGuiInitialized = false;
    if (!imGuiInitialized) {
        imGuiInitialized = true;

        // Get the game's window from the HDC
        hGameWindow = WindowFromDC(hdc);

        // Hook the window procedure
        hGameWindowProc = (WNDPROC)SetWindowLongPtr(hGameWindow, GWLP_WNDPROC, (LONG_PTR)windowProc_hook);

        // Initialize ImGui and OpenGL
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hGameWindow);
        ImGui_ImplOpenGL3_Init();
        ImGui::StyleColorsDark();
    }

    // If the overlay is shown, render ImGui and draw text
    if (menuShown) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        //Set the window size and position
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(405, 320), ImGuiCond_Once);

        // Alterar cores no ImGui usando PushStyleColor para configurar diferentes elementos da interface
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabSelected, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        // Draw simple text using ImGui
        ImGui::Begin("Auto-tool by clear-man");

        if (ImGui::BeginTabBar("MenuTabs")) {
            if (ImGui::BeginTabItem("PVE/PVP")) {
                // Display the player ID using ImGui
                if (playerID == 0x00)
                    ImGui::Text("Player ID: not found (cast Holy Aura on yourself)");
                else {
                    ImGui::Text("Player ID: %02X", playerID);
                    ImGui::SameLine();
                    if (ImGui::Button("Reset ID")) {
						playerID = 0x00;
					}
                }

                // Restaurar as cores originais
                ImGui::SliderInt("Attack speed (ms)", &ATK_SPEED, 100, 1500);

                //Add checkboxes for the features
                ImGui::Checkbox("Auto-PK", &AutoPK);
                ImGui::Checkbox("Auto-Kill", &AutoKill);
                ImGui::Checkbox("Auto-Loot", &AutoLoot);
                ImGui::Checkbox("Auto-Bless", &AutoBless);
                ImGui::Checkbox("Auto-Cure", &AutoCure);
                ImGui::Checkbox("Auto-Heal", &AutoHeal);
                ImGui::Checkbox("Auto-Aura", &AutoAura);

                if ((AutoPK || AutoKill || AutoLoot || AutoBless || AutoCure || AutoHeal || AutoAura) && !PVP_PVE)
                    PVP_PVE = true;
                if ((!AutoPK && !AutoKill && !AutoLoot && !AutoBless && !AutoCure && !AutoHeal && !AutoAura) && PVP_PVE)
                    PVP_PVE = false;

                // Add a button to toggle all
                if (ImGui::Button("Toggle all")) {
                    if (!PVP_PVE) {
                        AutoKill = true;
                        AutoLoot = true;
                        AutoPK = true;
                        AutoBless = true;
                        AutoCure = true;
                        AutoHeal = true;
                        AutoAura = true;
                        PVP_PVE = true;
                    }
                    else {
                        AutoKill = false;
                        AutoLoot = false;
                        AutoPK = false;
                        AutoBless = false;
                        AutoCure = false;
                        AutoHeal = false;
                        AutoAura = false;
                        PVP_PVE = false;
                    }
                }

                if(!AutoKill && !monsterQueue.empty()) {
					monsterQueue.clear();
				}

                if(!AutoPK && !playerQueue.empty()) {
                    playerQueue.clear();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Movement")) {
                static int x = 0, y = 0, z = 0, delay = 0;              // Campos para inserção
                static bool NewTeleportScript = false;
                static bool LoadTeleportScript = false;
                static bool NewAutoPathScript = false;
                static bool LoadAutoPathScript = false;

                // Botão "New Script" que mostra os campos de input
                if (ImGui::Button("New Teleport Script", ImVec2(150, 20))) {
                    //Redefine newTeleportSpot, x, y, z, delay
                    newTeleportSpot.clear();
                    LoadTeleportScript = false;
                    x = 0, y = 0, z = 0, delay = 0;
                    NewTeleportScript = true; // Exibe os campos ao clicar no botão
                }

                ImGui::SameLine();

                // Botão "Load Script" (você pode implementar a funcionalidade de carregar script aqui)
                if (ImGui::Button("Load Teleport Script", ImVec2(150, 20))) {
                    nfdchar_t* inPath = NULL;
                    nfdresult_t result = NFD_OpenDialog("txt", NULL, &inPath);

                    if (result == NFD_OKAY) {
                        newTeleportSpot.clear(); // Limpa os teleport spots atuais
                        NewTeleportScript = false;
                        LoadTeleportScript = true;
						loadTeleportsFromFile(newTeleportSpot, inPath); // Carrega o arquivo selecionado
						free(inPath); // Libera a memória alocada para o caminho
					}
                }

                // Exibe os campos X, Y, Z, Delay e o botão "Add Teleport" se o "New Script" foi pressionado
                if (NewTeleportScript) {
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::InputInt("X", &x, 0);       // Campo X
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::InputInt("Y", &y, 0);       // Campo Y
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::InputInt("Z", &z, 0);       // Campo Z
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::InputInt("Delay in ms", &delay, 0); // Campo Delay

                    // Botão "Add Teleport" que adiciona os valores inseridos ao vetor newTeleportSpot
                    if (ImGui::Button("Add Teleport", ImVec2(100, 20))) {
                        // Adiciona os valores inseridos ao vetor newTeleportSpot
                        newTeleportSpot.push_back({ x, y, z, delay });

                        // Reseta os campos para permitir nova inserção
                        x = 0;
                        y = 0;
                        z = 0;
                        delay = 0;
                    }

                    // Exibe os teleport spots já adicionados para visualização
                    ImGui::Separator();
                    ImGui::Text("Added Teleports:");
                    for (size_t i = 0; i < newTeleportSpot.size(); ++i) {
                        ImGui::Text("Teleport %d - X: %d Y: %d Z: %d - Delay: %dms", i + 1,
                            newTeleportSpot[i][0], newTeleportSpot[i][1],
                            newTeleportSpot[i][2], newTeleportSpot[i][3]);
                    }

                    // Botão "Save Script" que salva os teleport spots no arquivo teleports.txt
                    if (ImGui::Button("Save Teleport Script", ImVec2(150, 20))) {
                        nfdchar_t* outPath = NULL;
                        nfdresult_t result = NFD_SaveDialog("txt", NULL, &outPath);

                        if (result == NFD_OKAY) {
                            saveTeleportsToFile(newTeleportSpot, outPath); // Salva o arquivo no local selecionado
                            free(outPath); // Libera a memória alocada para o caminho
                            NewTeleportScript = false;
                            LoadTeleportScript = true;
                        }
                    }
                    
                }

                if (LoadTeleportScript) {
                    // Exibe os teleport spots já carregados para visualização
                    ImGui::Separator();
                    ImGui::Text("Teleport script:");
                    for (size_t i = 0; i < newTeleportSpot.size(); ++i) {
                        ImGui::Text("Teleport %d - X: %d Y: %d Z: %d - Delay: %dms", i + 1,
                            newTeleportSpot[i][0], newTeleportSpot[i][1],
                            newTeleportSpot[i][2], newTeleportSpot[i][3]);
                    }

                    if (!isTeleporting) {
                        if (ImGui::Button("Run Teleport Script", ImVec2(150, 20))) {
                            if (!isTeleporting) {
                                isTeleporting = true;
                                startTeleport(socketHandle);
                            }
                        }
                    }
                    else {
                        if (ImGui::Button("Stop Teleport Script", ImVec2(150, 20))) {
                            isTeleporting = false;
                        }
                    }
                }

                //Auto-path

                // Botão "New Auto-path Script" que mostra os campos de input
                if (ImGui::Button("New Auto-path Script", ImVec2(150, 20))) {
                    //Redefine newTeleportSpot, x, y, z
                    newAutoPathSpots.clear();
                    LoadAutoPathScript = false;
                    x = 0, y = 0, z = 0;
                    NewAutoPathScript = true; // Exibe os campos ao clicar no botão
                }

                ImGui::SameLine();

                // Botão "Load Auto-path Script"
                if (ImGui::Button("Load Auto-path Script", ImVec2(150, 20))) {
                    nfdchar_t* inPath = NULL;
                    nfdresult_t result = NFD_OpenDialog("txt", NULL, &inPath);

                    if (result == NFD_OKAY) {
                        newAutoPathSpots.clear(); // Limpa os autopath spots atuais
                        NewAutoPathScript = false;
                        LoadAutoPathScript = true;
                        loadTeleportsFromFile(newAutoPathSpots, inPath); // Carrega o arquivo selecionado
                        free(inPath); // Libera a memória alocada para o caminho
                    }
                }

                // Exibe os campos X, Y, Z e o botão "Add Coordinates" se o "New Script" foi pressionado
                if (NewAutoPathScript) {
                    ImGui::InputInt("X", &x, 0);       // Campo X
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::InputInt("Y", &y, 0);       // Campo Y
                    ImGui::SetNextItemWidth(50.0f);
                    ImGui::InputInt("Z", &z, 0);       // Campo Z

                    // Botão "Add Coordinates" que adiciona os valores inseridos ao vetor newAutoPathSpots
                    if (ImGui::Button("Add Coordinates", ImVec2(100, 20))) {
                        // Adiciona os valores inseridos ao vetor newAutoPathSpots
                        newAutoPathSpots.push_back({ x, y, z });

                        // Reseta os campos para permitir nova inserção
                        x = 0;
                        y = 0;
                        z = 0;
                    }

                    // Exibe os auto-path spots já adicionados para visualização
                    ImGui::Separator();
                    ImGui::Text("Added Coordinates:");
                    for (size_t i = 0; i < newAutoPathSpots.size(); ++i) {
                        ImGui::Text("Coordinates %d - X: %d Y: %d Z: %d", i + 1,
                            newAutoPathSpots[i][0], newAutoPathSpots[i][1],
                            newAutoPathSpots[i][2]);
                    }

                    // Botão "Save Auto-path Script" que salva os autopath spots no arquivo a escolha do usuário
                    if (ImGui::Button("Save Auto-path Script", ImVec2(150, 20))) {
                        nfdchar_t* outPath = NULL;
                        nfdresult_t result = NFD_SaveDialog("txt", NULL, &outPath);

                        if (result == NFD_OKAY) {
                            saveTeleportsToFile(newAutoPathSpots, outPath); // Salva o arquivo no local selecionado
                            free(outPath); // Libera a memória alocada para o caminho
                            NewAutoPathScript = false;
                            LoadAutoPathScript = true;
                        }
                    }

                }

                if (LoadAutoPathScript) {
                    // Exibe os autopath spots já carregados para visualização
                    ImGui::Separator();
                    ImGui::Text("Auto-path script:");
                    for (size_t i = 0; i < newAutoPathSpots.size(); ++i) {
                        ImGui::Text("Spot %d - X: %d Y: %d Z: %d", i + 1,
                            newAutoPathSpots[i][0], newAutoPathSpots[i][1],
                            newAutoPathSpots[i][2]);
                    }

                    if (!AutoPath) {
                        if (ImGui::Button("Run Auto-path Script", ImVec2(150, 20))) {
                            if (!AutoPath) {
                                AutoPath = true;
                            }
                        }
                    }
                    else {
                        if (ImGui::Button("Stop Auto-path Script", ImVec2(150, 20))) {
                            AutoPath = false;
                        }
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Camping")) {

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Spells")) {
                static int delayHolyArmour = 0, delayAntimagic = 0, delayEmpower = 0, delayHolyStrike = 0, delayNova = 0;

                ImGui::Checkbox("Holy Armour", &HolyArmour);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50.0f);
                ImGui::InputInt("Delay in ms##holyarmour", &delayHolyArmour, 0);
                if (HolyArmour && Spells) {
                    ImGui::SameLine();
                    if (ImGui::Button("Start##holyarmour")) {
                        startHolyArmour(socketHandle, delayHolyArmour);
                    }
                }

                ImGui::Checkbox("Antimagic", &Antimagic);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50.0f);
                ImGui::InputInt("Delay in ms##antimagic", &delayAntimagic, 0);
                if (Antimagic && Spells) {
                    ImGui::SameLine();
					if (ImGui::Button("Start##antimagic")) {
						startAntimagic(socketHandle, delayAntimagic);
					}
				}

                ImGui::Checkbox("Empower", &Empower);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50.0f);
                ImGui::InputInt("Delay in ms##empower", &delayEmpower, 0);
                if (Empower && Spells) {
					ImGui::SameLine();
					if (ImGui::Button("Start##empower")) {
						startEmpower(socketHandle, delayEmpower);
					}
				}

                ImGui::Checkbox("Holy Strike", &HolyStrike);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50.0f);
                ImGui::InputInt("Delay in ms##holystrike", &delayHolyStrike, 0);
                if (HolyStrike && Spells) {
                    ImGui::SameLine();
                    if (ImGui::Button("Start##holystrike")) {
						startHolyStrike(socketHandle, delayHolyStrike);
					}
                }

                ImGui::Checkbox("Nova", &Nova);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50.0f);
                ImGui::InputInt("Delay in ms##nova", &delayNova, 0);
                if (Nova && Spells) {
                    ImGui::SameLine();
                    if (ImGui::Button("Start##nova")) {
                        startNova(socketHandle, delayNova);
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Skills")) {

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Misc")) {

                ImGui::Text("Repair all at once");
                if (ImGui::Button("Repair gear")) {
                    
                }
                ImGui::SameLine();
                if(ImGui::Button("Repair jewelries")) {
					
				}

				ImGui::Separator();

                ImGui::Text("Attack Speed Calculator");
                ImGui::Combo("##atk_spd_calculator", &selectedWeapon, weapons, IM_ARRAYSIZE(weapons));
                ImGui::Checkbox("Enchanted", &Enchanted);
                ImGui::SameLine();
                ImGui::Checkbox("Slow debuff", &SlowDebuff);
                ImGui::Text("Attack speed: %dms", Enchanted ? (SlowDebuff ? static_cast<int>((weaponsAttackSpeed[selectedWeapon] - weaponsAttackSpeed[selectedWeapon] * 0.1)) + static_cast<int>((weaponsAttackSpeed[selectedWeapon] - weaponsAttackSpeed[selectedWeapon] * 0.1) * 0.5) : weaponsAttackSpeed[selectedWeapon] - static_cast<int>(weaponsAttackSpeed[selectedWeapon] * 0.1)) : SlowDebuff ? static_cast<int>(weaponsAttackSpeed[selectedWeapon] + weaponsAttackSpeed[selectedWeapon] * 0.5) : weaponsAttackSpeed[selectedWeapon]);

                ImGui::Separator();

                ImGui::Text("HP/Mana Regen Calculator");
                ImGui::Text("Constitution");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(50.0f);
				ImGui::InputFloat("##constitution", &Constitution, 0, 0, "%.0f");
				ImGui::SameLine();
				ImGui::Text("HP Regen: %.2f", Constitution / 70);
                ImGui::Text("Magic");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(50.0f);
				ImGui::InputFloat("##magic", &Magic, 0, 0, "%.0f");
				ImGui::SameLine();
				ImGui::Text("Mana Regen: %.2f", Magic / 40);

				ImGui::Separator();

				ImGui::Text("Bypasser");
				ImGui::Checkbox("Bypass The Watcher", &TheWatcher);
				ImGui::Checkbox("Bypass Bot Flag", &BotFlag);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("About")) {
                
				ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        float windowWidth = ImGui::GetWindowWidth();
        float windowHeight = ImGui::GetWindowHeight();
        float buttonWidth = ImGui::CalcTextSize("Hide").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float buttonHeight = ImGui::CalcTextSize("Hide").y + ImGui::GetStyle().FramePadding.y * 2.0f;

        // Define a posição do cursor para o botão "Encerrar"
        ImGui::SetCursorPosX(windowWidth - buttonWidth - ImGui::GetStyle().WindowPadding.x);
        ImGui::SetCursorPosY(windowHeight - buttonHeight - ImGui::GetStyle().WindowPadding.y);

        if(ImGui::Button("Hide")) {
			menuShown = false;
		}

        ImGui::End();

        // Pop the style colors to reset them
        ImGui::PopStyleColor(15);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // Call the original wglSwapBuffers to continue the game's rendering process
    return o_wglSwapBuffers(hdc);
}


// Function to install the WSARecv hook using Detours
bool InstallHook() {
    // Detour the WSARecv function in Ws2_32.dll
    originalWSARecv = (WSARecv_FUNC)GetProcAddress(GetModuleHandleA("Ws2_32.dll"), "WSARecv");
    originalWSASend = (WSASend_FUNC)GetProcAddress(GetModuleHandleA("Ws2_32.dll"), "WSASend");
    o_wglSwapBuffers = (wglSwapBuffers_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglSwapBuffers");

    if (!originalWSARecv || !originalWSASend || !o_wglSwapBuffers) {
        std::cerr << "Failed to get address of WSARecv or WSASend." << std::endl;
        return false;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)originalWSARecv, HookedWSARecv);  // Attach the hook for WSARecv
    DetourAttach(&(PVOID&)originalWSASend, HookedWSASend);  // Attach the hook for WSASend
    DetourAttach(&(PVOID&)o_wglSwapBuffers, Hooked_wglSwapBuffers);  // Attach the hook for wglSwapBuffers

    if (DetourTransactionCommit() != NO_ERROR) {
        std::cerr << "Failed to detour functions." << std::endl;
        return false;
    }

    std::cout << "Functions hooked successfully." << std::endl;
    return true;
}

// Function to uninstall the hooks
void UninstallHook() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)originalWSARecv, HookedWSARecv);  // Detach the hook for WSARecv
    DetourDetach(&(PVOID&)originalWSASend, HookedWSASend);  // Detach the hook for WSASend
    DetourDetach(&(PVOID&)o_wglSwapBuffers, Hooked_wglSwapBuffers);  // Detach the hook for wglSwapBuffers
    DetourTransactionCommit();
}

// DllMain: Called when the DLL is loaded/unloaded
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        //AllocConsole();  // Optional: Open a console for debugging output
        //freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);  // Redirect stdout to the console

        if (InstallHook()) {
            std::cout << "Hook installed successfully." << std::endl;
        }
        else {
            std::cerr << "Failed to install hook." << std::endl;
        }
        break;

    case DLL_PROCESS_DETACH:
        UninstallHook();  // Uninstall the hook when the DLL is unloaded
        //fclose(stdout);    // Close the console when done
        //FreeConsole();
        break;
    }
    return TRUE;
}