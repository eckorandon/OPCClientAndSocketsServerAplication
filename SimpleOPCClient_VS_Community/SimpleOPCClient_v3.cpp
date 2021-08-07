/* ======================================================================================================================== */
/*  CABECALHO*/
/*
	UNIVERSIDADE FEDERAL DE MINAS GERAIS

	Trabalho pratico sobre OPC e Sockets
	Sistemas distribuidos para automacao

	Professor:
	Luiz Themystokliz S. Mendes

	Alunos:
	Estevao Coelho Kiel de Oliveira     - 2016119416
	Lucas Costa Souza					- 2018013763

	Data: Agosto de 2021

	Aplicacao multithread que tem como objetivo a integracao de um servidor de sockets
	e um cliente OPC. O servidor de sockets se comunica com um cliente de sockets
	instalado em outra maquina em uma mesma rede local e o cliente OPC se comunica com um
	servidor OPC.

	O cliente de sockets funciona como um MES (Manufacturing Execution System), que 
	basicamente tem o objetivo de capturar de maneira contínua informações da planta 
	industrial e da area de gestao de negocios da empresa para a geracao de scheduling. 
	Ja o servidor OPC, do tipo classico, se comunica diretamente com os CLPs 
	(Controladores Logicos Programaveis) que sao os responsaveis pelo controle das linhas 
	de envase do processo de producao de bebidas nao alcoolicas.
	
	Desse modo, o MES deve ser abastecido de maneira continua com os dados do status da 
	planta que estão disponiveis no servidor OPC e os CLPs devem receber valores de setup 
	de producao provenientes do MES, esse valores correspondem a cada tipo diferente de 
	produto a ser envasado.
*/

/* ======================================================================================================================== */
/* BIBLIOTECA WINSOCK2*/
/*
	Para que o programa compile e funcione corretamente e necessario incluir a biblioteca 
	Winsock2 (Ws2_32.lib) no projeto. Para isso siga os seguintes passos:
	
	1. No Visual Studio Community Edition va em
	Project->Properties->Configuration Properties->Linker->Input
	
	2. No campo Additional Dependencies adicione a biblioteca Ws2_32.lib
*/

/* ======================================================================================================================== */
/* CREDITOS*/
/*
	A thread principal da nossa aplicacao e baseada na versao "Simple OPC Client"
	desenvolvida por Philippe Gras (CERN) e pode ser encontrada na data atual em
	http://pgras.home.cern.ch/pgras/OPCClientTutorial/

	A mesma foi alterada pelo professor Luiz T. S. Mendes do DELT/UFMG em 
	15 Setembro de 2011, onde ele introduziu duas classes em C++ para permitir
	que o cliente OPC requisite callback notifications de um servidor OPC.
	Também foi necessario incluir um loop de consumo de mensagens no programa 
	principal para permitir que o cliente OPC processe essas notificacoes.
	As classes em C++ implementam as interfaces para o cliente do 
	OPC DA 1.0 IAdviseSink e do OPC DA 2.0 IOPCDataCallback.
	Algumas funcoes para inicializacao e cancelamento das funcoes tambem
	foram implementadas.
*/

#undef UNICODE

/* ======================================================================================================================== */
/*  DEFINE AREA*/

#define WIN32_LEAN_AND_MEAN
#define OPC_SERVER_NAME			L"Matrikon.OPC.Simulation.1"
#define VT						VT_R4

#define DEFAULT_BUFLEN          64
#define DEFAULT_PORT            "5447"

#define TAMMSGSTATUS            42
#define TAMMSGACK               10

//#define REMOTE_SERVER_NAME L"your_path"

/* ======================================================================================================================== */
/*  INCLUDE AREA*/

#include <atlbase.h>											/*required for using the "_T" macro*/
#include <iostream>
#include <ObjIdl.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>												/*_getch()*/
#include <winsock2.h>
#include <ws2tcpip.h>
#include <math.h>                                               /*pow()*/

#include "opcda.h"
#include "opcerror.h"
#include "SimpleOPCClient_v3.h"
#include "SOCAdviseSink.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"

/* ======================================================================================================================== */
/* OTHERS*/

using namespace std;

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

/* ======================================================================================================================== */
/* DECLARACAO DO PROTOTIPO DE FUNCAO DA THREAD SECUNDARIA*/

DWORD WINAPI ServidorSockets(LPVOID index);

/* ======================================================================================================================== */
/*  DECLARACAO DAS VARIAVEIS GLOBAIS*/

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat (_T("OPCSTMFORMATDATATIME"));

wchar_t ITEM_ID[]=L"Saw-toothed Waves.Real4";

int nseq = 0;                                                   /*Valor referente ao numero sequensial da mensagem*/

/* ======================================================================================================================== */
/* THREAD PRIMARIA*/
/* LE OS VALORES DOS ITEMS EM UM SERVIDO OPC*/

void main(void) {
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
	IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
	IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface

	OPCHANDLE hServerGroup; // server handle to the group
	OPCHANDLE hServerItem;  // server handle to the item

	char buf[100];

	// Have to be done before using microsoft COM library:
	printf("Initializing the COM environment...\n");
	CoInitialize(NULL);

	// Let's instantiante the IOPCServer interface and get a pointer of it:
	printf("Instantiating the MATRIKON OPC Server for Simulation...\n");
	pIOPCServer = InstantiateServer(OPC_SERVER_NAME);
	
	// Add the OPC group the OPC server and get an handle to the IOPCItemMgt
	//interface:
	printf("Adding a group in the INACTIVE state for the moment...\n");
	AddTheGroup(pIOPCServer, pIOPCItemMgt, hServerGroup);

	// Add the OPC item. First we have to convert from wchar_t* to char*
	// in order to print the item name in the console.
    size_t m;
	wcstombs_s(&m, buf, 100, ITEM_ID, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
    AddTheItem(pIOPCItemMgt, hServerItem);
	
	int bRet;
	MSG msg;
	DWORD ticks1, ticks2;
	string mensagem;
    
	// Establish a callback asynchronous read by means of the IOPCDataCallback
	// (OPC DA 2.0) method. We first instantiate a new SOCDataCallback object and
	// adjusts its reference count, and then call a wrapper function to
	// setup the callback.
	IConnectionPoint* pIConnectionPoint = NULL; //pointer to IConnectionPoint Interface
	DWORD dwCookie = 0;
	SOCDataCallback* pSOCDataCallback = new SOCDataCallback();
	pSOCDataCallback->AddRef();

	printf("Setting up the IConnectionPoint callback connection...\n");
	SetDataCallback(pIOPCItemMgt, pSOCDataCallback, pIConnectionPoint, &dwCookie);

	// Change the group to the ACTIVE state so that we can receive the
	// server´s callback notification
	printf("Changing the group state to ACTIVE...\n");
    SetGroupActive(pIOPCItemMgt); 

	// Enter again a message pump in order to process the server´s callback
	// notifications, for the same reason explained before.
		
	ticks1 = GetTickCount();
	printf("Waiting for IOPCDataCallback notifications during 10 seconds...\n");
	do {
		mensagem = GetMessage( &msg, NULL, 0, 0 );
		printf("-%s", mensagem);
		if (!1){
			printf ("Failed to get windows message! Error code = %d\n", GetLastError());
			exit(0);
		}

		TranslateMessage(&msg); // This call is not really needed ...
		DispatchMessage(&msg);  // ... but this one is!

        ticks2 = GetTickCount();
	}
	while ((ticks2 - ticks1) < 10000);

	// Cancel the callback and release its reference
	printf("Cancelling the IOPCDataCallback notifications...\n");
    CancelDataCallback(pIConnectionPoint, dwCookie);
	//pIConnectionPoint->Release();
	pSOCDataCallback->Release();

	// Remove the OPC item:
	printf("Removing the OPC item...\n");
	RemoveItem(pIOPCItemMgt, hServerItem);

	// Remove the OPC group:
	printf("Removing the OPC group object...\n");
    pIOPCItemMgt->Release();
	RemoveGroup(pIOPCServer, hServerGroup);

	// release the interface references:
	printf("Removing the OPC server object...\n");
	pIOPCServer->Release();

	//close the COM library:
	printf ("Releasing the COM environment...\n");
	CoUninitialize();

	/*------------------------------------------------------------------------------*/
	/*Fecha handles*/
	CloseHandle(hServidorSockets);
}


////////////////////////////////////////////////////////////////////
// Instantiate the IOPCServer interface of the OPCServer
// having the name ServerName. Return a pointer to this interface
//
IOPCServer* InstantiateServer(wchar_t ServerName[])
{
	CLSID CLSID_OPCServer;
	HRESULT hr;

	// get the CLSID from the OPC Server Name:
	hr = CLSIDFromString(ServerName, &CLSID_OPCServer);
	_ASSERT(!FAILED(hr));


	//queue of the class instances to create
	LONG cmq = 1; // nbr of class instance to create.
	MULTI_QI queue[1] =
		{{&IID_IOPCServer,
		NULL,
		0}};

	//Server info:
	//COSERVERINFO CoServerInfo =
    //{
	//	/*dwReserved1*/ 0,
	//	/*pwszName*/ REMOTE_SERVER_NAME,
	//	/*COAUTHINFO*/  NULL,
	//	/*dwReserved2*/ 0
    //}; 

	// create an instance of the IOPCServer
	hr = CoCreateInstanceEx(CLSID_OPCServer, NULL, CLSCTX_SERVER,
		/*&CoServerInfo*/NULL, cmq, queue);
	_ASSERT(!hr);

	// return a pointer to the IOPCServer interface:
	return(IOPCServer*) queue[0].pItf;
}


/////////////////////////////////////////////////////////////////////
// Add group "Group1" to the Server whose IOPCServer interface
// is pointed by pIOPCServer. 
// Returns a pointer to the IOPCItemMgt interface of the added group
// and a server opc handle to the added group.
//
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt, 
				 OPCHANDLE& hServerGroup)
{
	DWORD dwUpdateRate = 0;
	OPCHANDLE hClientGroup = 0;

	// Add an OPC group and get a pointer to the IUnknown I/F:
    HRESULT hr = pIOPCServer->AddGroup(/*szName*/ L"Group1",
		/*bActive*/ FALSE,
		/*dwRequestedUpdateRate*/ 1000,
		/*hClientGroup*/ hClientGroup,
		/*pTimeBias*/ 0,
		/*pPercentDeadband*/ 0,
		/*dwLCID*/0,
		/*phServerGroup*/&hServerGroup,
		&dwUpdateRate,
		/*riid*/ IID_IOPCItemMgt,
		/*ppUnk*/ (IUnknown**) &pIOPCItemMgt);
	_ASSERT(!FAILED(hr));
}


//////////////////////////////////////////////////////////////////
// Add the Item ITEM_ID to the group whose IOPCItemMgt interface
// is pointed by pIOPCItemMgt pointer. Return a server opc handle
// to the item.
 
void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem)
{
	HRESULT hr;

	// Array of items to add:
	OPCITEMDEF ItemArray[1] =
	{{
	/*szAccessPath*/ L"",
	/*szItemID*/ ITEM_ID,
	/*bActive*/ TRUE,
	/*hClient*/ 1,
	/*dwBlobSize*/ 0,
	/*pBlob*/ NULL,
	/*vtRequestedDataType*/ VT,
	/*wReserved*/0
	}};

	//Add Result:
	OPCITEMRESULT* pAddResult=NULL;
	HRESULT* pErrors = NULL;

	// Add an Item to the previous Group:
	hr = pIOPCItemMgt->AddItems(1, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK){
		printf("Failed call to AddItems function. Error code = %x\n", hr);
		exit(0);
	}

	// Server handle for the added item:
	hServerItem = pAddResult[0].hServer;

	// release memory allocated by the server:
	CoTaskMemFree(pAddResult->pBlob);

	CoTaskMemFree(pAddResult);
	pAddResult = NULL;

	CoTaskMemFree(pErrors);
	pErrors = NULL;
}


///////////////////////////////////////////////////////////////////////////////
// Read from device the value of the item having the "hServerItem" server 
// handle and belonging to the group whose one interface is pointed by
// pGroupIUnknown. The value is put in varValue. 
//
void ReadItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue)
{
	// value of the item:
	OPCITEMSTATE* pValue = NULL;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**) &pIOPCSyncIO);

	// read the item value from the device:
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Read(OPC_DS_DEVICE, 1, &hServerItem, &pValue, &pErrors);
	_ASSERT(!hr);
	_ASSERT(pValue!=NULL);

	varValue = pValue[0].vDataValue;

	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	CoTaskMemFree(pValue);
	pValue = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}


///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem)
{
	// server handle of items to remove:
	OPCHANDLE hServerArray[1];
	hServerArray[0] = hServerItem;
	
	//Remove the item:
	HRESULT* pErrors; // to store error code(s)
	HRESULT hr = pIOPCItemMgt->RemoveItems(1, hServerArray, &pErrors);
	_ASSERT(!hr);

	//release memory allocated by the server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;
}


////////////////////////////////////////////////////////////////////////
// Remove the Group whose server handle is hServerGroup from the server
// whose IOPCServer interface is pointed by pIOPCServer
//
void RemoveGroup (IOPCServer* pIOPCServer, OPCHANDLE hServerGroup)
{
	// Remove the group:
	HRESULT hr = pIOPCServer->RemoveGroup(hServerGroup, FALSE);
	if (hr != S_OK){
		if (hr == OPC_S_INUSE)
			printf ("Failed to remove OPC group: object still has references to it.\n");
		else printf ("Failed to remove OPC group. Error code = %x\n", hr);
		exit(0);
	}
}


/* ======================================================================================================================== */
/* THREAD SECUNDARIA*/
/* */

DWORD WINAPI ServidorSockets(LPVOID index) {
	/*------------------------------------------------------------------------------*/
	/*Declarando variaveis locais*/
	WSADATA             wsaData;

	SOCKET              ListenSocket = INVALID_SOCKET;
	SOCKET              ClientSocket = INVALID_SOCKET;

	struct addrinfo* result = NULL;
	struct addrinfo     hints;

	SYSTEMTIME          SystemTime;

	int                 iResult,
		iSendResult,
		k = 0,
		recvbuflen = DEFAULT_BUFLEN;

	char                recvbuf[DEFAULT_BUFLEN],
		msgstatus[TAMMSGSTATUS + 1] = "99/#######/##/####.#/####.#/#####",
		msgack[TAMMSGACK + 1] = "22/#######";

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
		else if (iResult == SOCKET_ERROR) {
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