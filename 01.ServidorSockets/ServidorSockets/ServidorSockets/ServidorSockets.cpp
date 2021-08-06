#undef UNICODE

/* ======================================================================================================================== */
/* DEFINE AREA*/

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_BUFLEN          128
#define DEFAULT_PORT            "5447"

#define TAMMSGSTATUS            42
#define TAMMSGACK               10

/* ======================================================================================================================== */
/* INCLUDE AREA*/

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>                                               /*pow()*/

/* ======================================================================================================================== */
/* XXXX*/

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
//#pragma comment (lib, "Mswsock.lib")

/* ======================================================================================================================== */
/* DECLARACAO DO PROTOTIPO DE FUNCAO DA THREAD SECUNDARIA*/

DWORD WINAPI ServidorSockets(LPVOID index);

/* ======================================================================================================================== */
/* DECLARACAO DAS VARIAVEIS GLOBAIS*/

int nseq = 0;                                                   /*Valor referente ao numero sequensial da mensagem*/

/* ======================================================================================================================== */
/* THREAD PRIMARIA*/

int main() {
    /*------------------------------------------------------------------------------*/
    /*Nomeando terminal*/
    SetConsoleTitle("TERMINAL PRINCIPAL");

    /*------------------------------------------------------------------------------*/
    /*Criando thread secundaria*/
    HANDLE hServidorSockets;

    DWORD dwServidorSocketsId;
    DWORD dwExitCode = 0;

    int i = 1;

    hServidorSockets = CreateThread(NULL, 0, ServidorSockets, (LPVOID)i, 0, &dwServidorSocketsId);
    if (hServidorSockets) printf("Thread %d criada com Id = %0d \n", i, dwServidorSocketsId);

    /*------------------------------------------------------------------------------*/
    /*Teste*/

    while (true) {
        Sleep(10000);
    }

    /*------------------------------------------------------------------------------*/
    /*Fecha handles*/

    CloseHandle(hServidorSockets);
}

/* ======================================================================================================================== */
/* THREAD SECUNDARIA*/

DWORD WINAPI ServidorSockets(LPVOID index) {
    /*------------------------------------------------------------------------------*/
    /*Declarando variaveis locais*/
    WSADATA             wsaData;

    SOCKET              ListenSocket    = INVALID_SOCKET;
    SOCKET              ClientSocket    = INVALID_SOCKET;

    struct addrinfo*    result          = NULL;
    struct addrinfo     hints;

    int                 iResult, 
                        iSendResult,
                        k               = 0,
                        recvbuflen      = DEFAULT_BUFLEN;
    
    char                recvbuf     [DEFAULT_BUFLEN],
                        msgstatus   [TAMMSGSTATUS + 1]  = "99/#######/##/####.#/####.#/#####",
                        msgack      [TAMMSGACK + 1]     = "22/#######";

    /*------------------------------------------------------------------------------*/
    /*Configuracoes do servidor*/

    /*Carrega a biblioteca winsock versao 2.2*/
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("Falha na inicializacao do Winsock 2! Erro = %d\n", WSAGetLastError());
        WSACleanup();
        exit(0);
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    /*Definindo o endereco do servidor e porta para conexao*/
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("Falha em getaddrinfo()! Erro = %d\n", WSAGetLastError());
        WSACleanup();
        exit(0);
    }

    /*Criacao do socket para conexao com o servidor*/
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("Falha na inicializacao do socket! Erro = %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        exit(0);
    }

    /*Vincula o socket a porta definida*/
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("Falha na vinculacao do socket a porta TCP! Erro = %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        exit(0);
    }

    freeaddrinfo(result);

    /*Colocando a porta TCP em modo de escuta - Backlog de 5 eh o valor tipico*/
    iResult = listen(ListenSocket, 5);
    if (iResult == SOCKET_ERROR) {
        printf("Falha ao colocar a porta TCP no modo de escuta! Erro = %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        exit(0);
    }

    while (true) {
        /*Aguarda a conexao de um cliente de sockets entao aceita*/
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("Falha ao aceitar a conexao com o cliente de sockets! Erro = %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            exit(0);
        }

        /*Recebe e envia mensagens ate que a conexao seja encerrada*/
        do {
            iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                /*Caso tenha solicitado status da planta*/
                if (iResult == 10) {
                    
                    //Imprimir mensagem recebida

                    nseq = (nseq + 2) % 9999999;
                    
                    for (int j = 3; j < 10; j++) {
                        k = nseq / pow(10, (9 - j));
                        k = k % 10;
                        msgstatus[j] = k + '0';
                    }
                    
                    /*Envia mensagem com dados do status da planta*/
                    iSendResult = send(ClientSocket, msgstatus, TAMMSGSTATUS, 0);
                    if (iSendResult == SOCKET_ERROR) {
                        printf("Falha ao enviar a mensagem com dados do status da planta! Erro = %d\n", WSAGetLastError());
                        closesocket(ClientSocket);
                        WSACleanup();
                        exit(0);
                    }
                    
                    //Imprimir mensagem enviada 
                }

                /*Caso tenha solicitado confirmcao*/
                if (iResult == 33) {

                    nseq = (nseq + 2) % 9999999;

                    for (int j = 3; j < 10; j++) {
                        k = nseq / pow(10, (9 - j));
                        k = k % 10;
                        msgack[j] = k + '0';
                    }

                    // Echo the buffer back to the sender
                    iSendResult = send(ClientSocket, msgack, TAMMSGACK, 0);
                    if (iSendResult == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        closesocket(ClientSocket);
                        WSACleanup();
                        return 1;
                    }
                    printf("Bytes sent: %d\n", iSendResult);
                }
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