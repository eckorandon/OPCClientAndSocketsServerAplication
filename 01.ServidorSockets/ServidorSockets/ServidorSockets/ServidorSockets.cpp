#undef UNICODE

/* ======================================================================================================================== */
/*  DEFINE AREA*/

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "5447"
#define TAMMSGSTATUS 42

/* ======================================================================================================================== */
/*  INCLUDE AREA*/

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

/* ======================================================================================================================== */
/* XXXX*/

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
//#pragma comment (lib, "Mswsock.lib")

/* ======================================================================================================================== */
/* DECLARACAO DO PROTOTIPO DE FUNCAO DAS THREADS SECUNDARIAS*/

DWORD WINAPI ServidorSockets(LPVOID index);

/* ======================================================================================================================== */
/* DECLARACAO DAS VARIAVEIS GLOBAIS*/

/* ======================================================================================================================== */
/* THREAD PRIMARIA*/

int main() {
    /*------------------------------------------------------------------------------*/
    /*Nomeando terminal*/
    SetConsoleTitle("TERMINAL PRINCIPAL");

    /*------------------------------------------------------------------------------*/
    /*Thread secundaria*/
    HANDLE hServidorSockets;

    DWORD dwServidorSocketsId;
    DWORD dwExitCode = 0;

    int i = 1;

    hServidorSockets = CreateThread(NULL, 0, ServidorSockets, (LPVOID)i, 0, &dwServidorSocketsId);
    if (hServidorSockets) printf("Thread %d criada com Id = %0d \n", i, dwServidorSocketsId);

    /*------------------------------------------------------------------------------*/
    /*Teste*/

    while (true) {
        printf("1\n");
        Sleep(10000);
    }

    /*------------------------------------------------------------------------------*/
    /*Fecha handles*/

    CloseHandle(hServidorSockets);
}

/* ======================================================================================================================== */
/* THREAD SECUNDARIA*/

DWORD WINAPI ServidorSockets(LPVOID index) {
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    char msgsetup[TAMMSGSTATUS + 1] = "99/0000002/02/0050.0/0040.0/00050";
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    while (true) {
        // Accept a client socket
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }

        // Receive until the peer shuts down the connection
        do {

            iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                printf("Bytes received: %d\n", iResult);

                // Echo the buffer back to the sender
                iSendResult = send(ClientSocket, msgsetup, TAMMSGSTATUS, 0);
                if (iSendResult == SOCKET_ERROR) {
                    printf("send failed with error: %d\n", WSAGetLastError());
                    closesocket(ClientSocket);
                    WSACleanup();
                    return 1;
                }
                printf("Bytes sent: %d\n", iSendResult);
            }
            else if (iResult == 0)
                printf("Connection closing...\n");
            else {
                printf("recv failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }

        } while (iResult > 0);
    }

    // No longer need server socket
    closesocket(ListenSocket);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    /*------------------------------------------------------------------------------*/
    /*Finalizando a thread servidor de sockets*/
    printf("Finalizando thread servidor de sockets\n");
    ExitThread((DWORD)index);
}