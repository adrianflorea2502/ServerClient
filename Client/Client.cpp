/* Florea Adrian Workshop MP */

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <chrono>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

enum MessageType { TYPEHELLO = 1, TYPEPING = 2, TYPEPONG = 3 };

int iResult = 0;
WSADATA wsaData;
u_long iMode = 1;

SOCKET SendSocket = INVALID_SOCKET;
sockaddr_in RecvAddr;

unsigned short Port = 27015;

char SendBuf[1024] = "Ne jucam cu sockets?";
int BufLen = 1024;

char RecvBuf[1024];

struct sockaddr_in SenderAddr;
int SenderAddrSize = sizeof(SenderAddr);

bool isConnected = false;

char packetMessageHello[1024];
char packetMessagePing[1024];
char packetMessagePong[1024];

char messageType;

uint64_t timeoutStart;
uint64_t timeoutFinish;

std::chrono::high_resolution_clock::time_point start;
std::chrono::high_resolution_clock::time_point finish;

bool CleanUp() {

    //---------------------------------------------
    // When the application is finished sending, close the socket.
    wprintf(L"[client] Finished sending. Closing socket.\n");
    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"[client] closesocket failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    //---------------------------------------------
    // Clean up and quit.
    wprintf(L"[client] Exiting.\n");
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

        printf("[client] Timed out!\n");
        CleanUp();
        exit(0);
    }
}

bool Initialize() {

    timeoutStart = EpochTime();

    //----------------------
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"[client] WSAStartup failed with error: %d\n", iResult);
        return false;
    }

    //---------------------------------------------
    // Create a socket for sending data
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        wprintf(L"[client] socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return false;
    }

    iResult = ioctlsocket(SendSocket, FIONBIO, &iMode);
    if (iResult != NO_ERROR) {
        printf("[client] ioctlsocket failed with error: %ld\n", iResult);
        return false;
    }
    //---------------------------------------------
    // Set up the RecvAddr structure with the IP address of
    // the receiver (in this example case "127.0.0.1")
    // and the specified port number.
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
    RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    return true;
}

void SendData(char* message) {

    //---------------------------------------------
    // Send a datagram to the receiver
    wprintf(L"[client] Sending a message to the server...\n");
    iResult = sendto(SendSocket,
        message, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));

    if (iResult == SOCKET_ERROR) {
        wprintf(L"[client] sendto failed with error: %d\n", WSAGetLastError());
        //closesocket(SendSocket);
        //WSACleanup();
    }
}

void RecvData() {

    //-----------------------------------------------
    // Call the recvfrom function to receive datagrams
    // on the bound socket.
    wprintf(L"[client] Receiving from server...\n");
    iResult = recvfrom(SendSocket,
        RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);

    if (iResult == SOCKET_ERROR) {

        if (iResult == WSAEWOULDBLOCK) return;

        wprintf(L"[client] recvfrom failed with error %d\n", WSAGetLastError());
    }

    if (RecvBuf[0] == TYPEHELLO) {

        messageType = TYPEHELLO;
        isConnected = true;
    }
    else if (RecvBuf[0] == TYPEPONG) {

        messageType = TYPEPONG;
    }
    else if (RecvBuf[0] == TYPEPING) {

        messageType = TYPEPING;
    }
    if (strlen(RecvBuf) > 0) timeoutStart = EpochTime();

    printf("[client] Received: %s\n", RecvBuf);
    memcpy(RecvBuf, "", 1024);
}

void Update() {

    if (isConnected == false) {

        SendData(packetMessageHello);
    }
    else {

        SendData(packetMessagePong);
    }

    RecvData();
    ComputeTimeout();
}

int main()
{
    bool ret;

    ret = Initialize();

    SetupMessages();

    if (ret == false) {

        return -1;
    }

    while (true) {

        Update();
        Sleep(1000);
    }
    ret = CleanUp();

    return 0;
}