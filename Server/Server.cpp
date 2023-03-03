/* Florea Adrian Workshop MP */

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <set>
#include <string.h>
#include <chrono>
#include <list>
#include <iterator>
#include <vector>

using namespace std;

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

enum MessageType { TYPEHELLO = 1, TYPEPING = 2, TYPEPONG = 3 };

int iResult = 0;
u_long iMode = 1;

WSADATA wsaData;

SOCKET RecvSocket;
struct sockaddr_in RecvAddr;

unsigned short Port = 27015;

char RecvBuf[1024];
char SendBuf[1024] = "ACK";
int BufLen = 1024;

struct sockaddr_in SenderAddr;
int SenderAddrSize = sizeof(SenderAddr);

char packetMessageHello[1024];
char packetMessagePing[1024];
char packetMessagePong[1024];

char messageType;

int pongCount = 0;
int avgDelay = 0;
vector<int> delayList;

uint64_t start;
uint64_t finish;

uint64_t timeoutStart;
uint64_t timeoutFinish;

std::chrono::high_resolution_clock::time_point delayStart;
std::chrono::high_resolution_clock::time_point delayFinish;

typedef struct Message {
    char buffer[1024];
};
vector<Message> messageList;

struct Compare final
{
    bool operator()(const sockaddr_in& lhs, const sockaddr_in& rhs) const noexcept
    {
        return ntohl(lhs.sin_port) < ntohl(rhs.sin_port); // comparision logic
    }
};

set<struct sockaddr_in, Compare> client_set;

bool CleanUp() {

    //-----------------------------------------------
    // Close the socket when finished receiving datagrams
    wprintf(L"[server] Finished receiving. Closing socket.\n");
    iResult = closesocket(RecvSocket);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"[server] closesocket failed with error %d\n", WSAGetLastError());
        return false;
    }

    //-----------------------------------------------
    // Clean up and exit.
    wprintf(L"[server] Exiting.\n");
    WSACleanup();
}

void SetupMessages() {

    packetMessageHello[0] = TYPEHELLO;
    packetMessagePing[0] = TYPEPING;
    packetMessagePong[0] = TYPEPONG;

    memcpy(packetMessageHello + 1, "hello", sizeof("hello"));
    memcpy(packetMessagePing + 1, "ping", sizeof("ping"));
    memcpy(packetMessagePong + 1, "pong", sizeof("pong"));
}

int EpochTime() {

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void ComputeTimeout() {

    timeoutFinish = EpochTime();

    if (timeoutFinish - timeoutStart >= 15000) {

        printf("[server] Timed out!\n");
        CleanUp();

        for (int i = 0; i < messageList.size(); i++) {

            printf("%s\n", messageList[i].buffer);
        }

        exit(0);
    }
}

void ComputeAverageDelay() {

    int i = 0;

    for (i = 0; i < delayList.size(); i++) {

        avgDelay += delayList[i];
    }
    if (i != 0) {
        avgDelay /= i;
    }
}

void ComputePongCount() {

    finish = EpochTime();

    if (finish - start >= 10000) {

        start = EpochTime();
        ComputeAverageDelay();
        printf("[server] Number of 'pongs': %d with AVG. delay: %dms\n", pongCount, avgDelay);
        pongCount = 0;
        avgDelay = 0;
        delayList.clear();
    }
}

bool Initialize() {

    timeoutStart = EpochTime();
    start = EpochTime();
    delayStart = std::chrono::high_resolution_clock::now();
    delayFinish = std::chrono::high_resolution_clock::now();

    //-----------------------------------------------
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"[server] WSAStartup failed with error %d\n", iResult);
        return false;
    }
    //-----------------------------------------------
    // Create a receiver socket to receive datagrams
    RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (RecvSocket == INVALID_SOCKET) {
        wprintf(L"[server] socket failed with error %d\n", WSAGetLastError());
        return false;
    }

    iResult = ioctlsocket(RecvSocket, FIONBIO, &iMode);
    if (iResult != NO_ERROR) {
        printf("[server] ioctlsocket failed with error: %ld\n", iResult);
        return false;
    }
    //-----------------------------------------------
    // Bind the socket to any address and the specified port.
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
    RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    iResult = bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult != 0) {
        wprintf(L"[server] bind failed with error %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

void SendData(char* message) {

    //---------------------------------------------
    // Send a datagram to the receiver
    wprintf(L"[server] Sending a message to the client...\n");
    iResult = sendto(RecvSocket,
        message, BufLen, 0, (SOCKADDR*)&SenderAddr, sizeof(SenderAddr));

    if (iResult == SOCKET_ERROR) {
        wprintf(L"[server] sendto failed with error: %d\n", WSAGetLastError());
        //closesocket(RecvSocket);
        //WSACleanup();
    }
}

void RecvData() {

    //-----------------------------------------------
    // Call the recvfrom function to receive datagrams
    // on the bound socket.
    wprintf(L"[server] Receiving datagrams...\n");
    iResult = recvfrom(RecvSocket,
        RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);

    if (iResult == SOCKET_ERROR) {

        if (iResult == WSAEWOULDBLOCK) return;

        wprintf(L"[server] recvfrom failed with error %d\n", WSAGetLastError());
    }
    if (RecvBuf[0] == TYPEHELLO) {

        messageType = TYPEHELLO;
    }
    else if (RecvBuf[0] == TYPEPONG) {

        messageType = TYPEPONG;
        pongCount++;
    }
    else if (RecvBuf[0] == TYPEPING) {

        messageType = TYPEPING;
    }
    if (strlen(RecvBuf) > 0) {

        timeoutStart = EpochTime();
        Message aux;
        memcpy(aux.buffer, RecvBuf, BufLen);
        messageList.push_back(aux);
    }

    printf("[server] Received: %s\n", RecvBuf);
    memcpy(RecvBuf, "", 1024);
}

void AddToClientSet() {

    if (client_set.find(SenderAddr) != client_set.end()) {

        printf("[server] Address already in set!\n");
        return;
    }
    client_set.insert(SenderAddr);

    char ipBuf[1024];

    inet_ntop(AF_INET, &SenderAddr.sin_addr, ipBuf, sizeof(ipBuf));

    printf("[server] Added to ip list: %s\n", ipBuf);
}

void Update() {

    delayFinish = std::chrono::high_resolution_clock::now();

    auto duration = delayFinish - delayStart;
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    //printf("%d\n", durationMs);
    delayList.push_back((int)durationMs);

    ComputePongCount();

    RecvData();
    ComputeTimeout();

    delayStart = std::chrono::high_resolution_clock::now();


    if (messageType == TYPEHELLO) {

        SendData(packetMessageHello);
    }
    else {

        SendData(packetMessagePing);
    }

    // Store client address
    AddToClientSet();
}

int main()
{
    bool ret;

    ret = Initialize();

    SetupMessages();

    if (ret == true) {

        while (true) {

            Update();
            Sleep(1000);

            //break;
        }
    }

    ret = CleanUp();

    return 0;
}
