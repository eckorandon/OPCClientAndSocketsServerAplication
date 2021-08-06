#undef UNICODE

/* ======================================================================================================================== */
/* DEFINE AREA*/

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_BUFLEN          64
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

    SYSTEMTIME          SystemTime;

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

            /*Exibe a hora corrente*/
            GetSystemTime(&SystemTime);
            printf("SISTEMA DE CONTROLE: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\n",
                    SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
                    SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);

            /*Caso tenha solicitado status da planta*/
            if (iResult == 10) {
                    
                /*Imprime mensagem recebida em verde*/
                printf("\x1b[32m");
                printf("Msg de requisicao de dados recebida do MES\n%.10s\n\n", recvbuf);
                printf("\x1b[0m");

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
                    
                /*Imprime mensagem enviada em amarelo*/
                printf("\x1b[33m");
                printf("Mensagem de status da planta enviada ao MES\n%s\n\n", msgstatus);
                printf("\x1b[0m");
            }

            /*Caso tenha solicitado confirmcao*/ 
            if (iResult == 33) {

                /*Imprime mensagem recebida em cyan*/
                printf("\x1b[36m");
                printf("Mensagem de setup de quipamentos recebida do MES\n%.33s\n\n", recvbuf);
                printf("\x1b[0m");

                nseq = (nseq + 2) % 9999999;

                for (int j = 3; j < 10; j++) {
                    k = nseq / pow(10, (9 - j));
                    k = k % 10;
                    msgack[j] = k + '0';
                }

                /*Envia mensagem de confirmacao (ACK)*/
                iSendResult = send(ClientSocket, msgack, TAMMSGACK, 0);
                if (iSendResult == SOCKET_ERROR) {
                    printf("Falha ao enviar a mensagem de confirmacao (ACK)! Erro = %d\n", WSAGetLastError());
                    closesocket(ClientSocket);
                    WSACleanup();
                    exit(0);
                }
                    
                /*Imprime mensagem enviada em magenta*/
                printf("\x1b[35m");
                printf("Mensagem de ACK enviada ao MES\n%s\n\n", msgack);
                printf("\x1b[0m");
            }
        }
        else if (iResult == 0) {
            printf("Nenhum dado recebi, encerrando servidor de sockets!");
        }
        else if (iResult == SOCKET_ERROR){
            printf("Falha ao receber dados do cliente sockets - recv()! Erro = %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            exit(0);
        }
    } while (iResult > 0);

    /*Fechando o socket*/
    closesocket(ListenSocket);

    /*Desligando a conexao*/
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("Falha ao desligar a conexao! Erro = %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        exit(0);
    }

    /*Fechando o socket*/
    closesocket(ClientSocket);

    /*Cleanup*/
    WSACleanup();

    /*------------------------------------------------------------------------------*/
    /*Finalizando a thread servidor de sockets*/
    printf("Finalizando thread servidor de sockets\n");
    ExitThread((DWORD)index);
}