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
	OPC DA 1.0 IAdviseSink (removida nesta aplicação) e do OPC DA 2.0 
	IOPCDataCallback. Algumas funcoes para inicializacao e cancelamento das 
	funcoes tambem foram implementadas e alteradas pela dupla.
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
#define TAMMSGSETUP				33
#define TAMMSGSOL				10

//#define REMOTE_SERVER_NAME L"your_path"

/* ======================================================================================================================== */
/*  INCLUDE AREA*/

#include <atlbase.h>											/*required for using the "_T" macro*/
#include <iostream>
#include <ObjIdl.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>												/*_getch()*/
#include <winsock2.h>
#include <ws2tcpip.h>
#include <math.h>                                               /*pow()*/
#include <mutex>

#include "opcda.h"
#include "opcerror.h"
#include "SimpleOPCClient_v3.h"
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
void decode();

/* ======================================================================================================================== */
/*  DECLARACAO DAS VARIAVEIS GLOBAIS*/

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat(_T("OPCSTMFORMATDATATIME"));

wchar_t ITEM_ID1[] = L"Random.Int1";
wchar_t ITEM_ID2[] = L"Random.Int2";
wchar_t ITEM_ID3[] = L"Random.Int4";
wchar_t ITEM_ID4[] = L"Random.Real4";
wchar_t ITEM_ID5[] = L"Saw-toothed Waves.Int2";
wchar_t ITEM_ID6[] = L"Saw-toothed Waves.Real4";

wchar_t ITEM_CLIENT_ID1[] = L"Bucket Brigade.Int1";
wchar_t ITEM_CLIENT_ID2[] = L"Bucket Brigade.Int4";
wchar_t ITEM_CLIENT_ID3[] = L"Bucket Brigade.Real4";
wchar_t ITEM_CLIENT_ID4[] = L"Bucket Brigade.Real8";

int nseq = 0;                                                   /*Valor referente ao numero sequensial da mensagem*/

VARIANT *sdadoLeitura;
OPCHANDLE *shandleLeitura;

SYSTEMTIME SystemTime;

char* messageOPCToTCP = new char;

IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface

OPCHANDLE hClientItem1;
OPCHANDLE hClientItem2;
OPCHANDLE hClientItem3;
OPCHANDLE hClientItem4;

char	msgstatus[TAMMSGSTATUS + 1] = "99/#######/#####.#/#####.#/###/###/###/###",
		msgack[TAMMSGACK + 1] = "22/#######",
		msgsetup[TAMMSGSETUP + 1] = "77/#######/##/####.#/####.#/#####",
		msgsol[TAMMSGSOL + 1] = "00/#######";

/* ======================================================================================================================== */
/* HANDLE MUTEX*/

HANDLE hMutexStatus;
HANDLE hMutexSetup;

/* ======================================================================================================================== */
/* HANDLE EVENTOS*/

HANDLE hEvent;

/* ======================================================================================================================== */
/* THREAD PRIMARIA*/
/* INICIALIZACAO E CONFIGURACAO DO SERVIDOR OPC*/
/* INICIALIZACAO DA THREAD SECUNDARIA*/
/* INICIALIZACAO DE MUTEX*/
/* LEITURA E ESCRITA DE MULTIPLOS ITENS NO SERVIDOR OPC*/
/* PASSAGEM DE PARAMETROS PARA O SERVIDOR DE SOCKETS*/

void main(void) {
	/*------------------------------------------------------------------------------*/
	/*Nomeando terminal*/
	SetConsoleTitle("TERMINAL PRINCIPAL");

	/*------------------------------------------------------------------------------*/
	/*Criando thread secundaria*/
	HANDLE hServidorSockets, hEscritaSincrona;

	DWORD dwServidorSocketsId, dwEscritaSincronaId;
	DWORD dwExitCode = 0;

	DWORD ret;

	int i = 1, nTipoEvento;

	i = 1;
	hServidorSockets = CreateThread(NULL, 0, ServidorSockets, (LPVOID)i, 0, &dwServidorSocketsId);
	if (hServidorSockets) printf("Thread %d criada com Id = %0d \n", i, dwServidorSocketsId);

	/*------------------------------------------------------------------------------*/
	/*Criando objetos do tipo mutex*/
	hMutexStatus = CreateMutex(NULL, FALSE, "MutexStatus");
	GetLastError();

	hMutexSetup = CreateMutex(NULL, FALSE, "MutexSetup");
	GetLastError();

	/*------------------------------------------------------------------------------*/
	/*Criando objetos do tipo eventos*/
	hEvent = CreateEvent(NULL, FALSE, FALSE, "Escrita");
	GetLastError();

	/*------------------------------------------------------------------------------*/

	OPCHANDLE hServerGroup;		// server handle to the group

	OPCHANDLE hServerItem1;		// server handle to the item
	OPCHANDLE hServerItem2;		// server handle to the item
	OPCHANDLE hServerItem3;		// server handle to the item
	OPCHANDLE hServerItem4;		// server handle to the item
	OPCHANDLE hServerItem5;		// server handle to the item
	OPCHANDLE hServerItem6;		// server handle to the item
	
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
	wcstombs_s(&m, buf, 100, ITEM_ID1, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_ID2, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_ID3, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_ID4, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_ID5, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_ID6, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_CLIENT_ID1, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_CLIENT_ID2, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_CLIENT_ID3, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	wcstombs_s(&m, buf, 100, ITEM_CLIENT_ID4, _TRUNCATE);
	printf("Adding the item %s to the group...\n", buf);
	AddTheItem(pIOPCItemMgt, hServerItem1, hServerItem2, hServerItem3, hServerItem4, hServerItem5, hServerItem6, hClientItem1, hClientItem2, hClientItem3, hClientItem4);

	int bRet;
	MSG msg;
    
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
	
	printf("Waiting for IOPCDataCallback notifications...\n\n");
	char buffer[100];
	VARIANT var;
	::VariantInit(&var);
	var.vt = VT_I4;
	var.iVal = 0;
	VarToStr(var, buffer);

	while (true) {
		bRet = GetMessage(&msg, NULL, 0, 0);
		if (!bRet) {
			printf("Failed to get windows message! Error code = %d\n", GetLastError());
			exit(0);
		}

		TranslateMessage(&msg); // This call is not really needed ...
		DispatchMessage(&msg);  // ... but this one is!
		
		sdadoLeitura = pSOCDataCallback->sendValues();
		shandleLeitura = pSOCDataCallback->sendHandles();

		for (int a = 0; a < 6; a++) {
			VarToStr(sdadoLeitura[a], buf);
		}
		
		ret = WaitForSingleObject(hEvent, 1);
		GetLastError();

		nTipoEvento = ret - WAIT_OBJECT_0;

		if (nTipoEvento == 0) {

			//msgsetup tem os dados

			printf("\nEscrevendo o valor %s na variavel com o handle %d\n\n", buffer, (int)hClientItem1);
			WriteItem(pIOPCItemMgt, hClientItem1, var);

			var.iVal++;
			VarToStr(var, buffer);

			printf("\nEscrevendo o valor %s na variavel com o handle %d\n\n", buffer, (int)hClientItem2);
			WriteItem(pIOPCItemMgt, hClientItem2, var);

			var.iVal++;
			VarToStr(var, buffer);

			printf("\nEscrevendo o valor %s na variavel com o handle %d\n\n", buffer, (int)hClientItem3);
			WriteItem(pIOPCItemMgt, hClientItem3, var);

			var.iVal++;
			VarToStr(var, buffer);

			printf("\nEscrevendo o valor %s na variavel com o handle %d\n\n", buffer, (int)hClientItem4);
			WriteItem(pIOPCItemMgt, hClientItem4, var);

			nTipoEvento = -1;
			ret = 1;
		}


		var.iVal++;
		VarToStr(var, buffer);

		/*********************************************************************************/
		// Tratamento dos dados
		
		decode();

		/*Exibe a hora corrente*/
		GetSystemTime(&SystemTime);
		printf("\x1B[31mSISTEMA DE CONTROLE: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\x1B[0m\n",
			SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
			SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);
		printf("Dados do status da planta lidos do Servidor OPC\n##/#######/%s\n\n", messageOPCToTCP);

		ret = WaitForSingleObject(hMutexStatus, 1);
		GetLastError();

		nTipoEvento = ret - WAIT_OBJECT_0;

		if (nTipoEvento == 0) {
			for (int j = 11; j <= TAMMSGSTATUS; j++) {
				msgstatus[j] = messageOPCToTCP[j - 11];
			}

			nTipoEvento = -1;
			ret = 1;
		}
		
		ret = ReleaseMutex(hMutexStatus);
		GetLastError();
	}

	// Cancel the callback and release its reference
	printf("Cancelling the IOPCDataCallback notifications...\n");
    CancelDataCallback(pIConnectionPoint, dwCookie);
	//pIConnectionPoint->Release();
	pSOCDataCallback->Release();

	// Remove the OPC item:
	printf("Removing the OPC item...\n");
	RemoveItem(pIOPCItemMgt, hServerItem1, hServerItem2, hServerItem3, hServerItem4, hServerItem5, hServerItem6, hClientItem1, hClientItem2, hClientItem3, hClientItem4);

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
	CloseHandle(hMutexStatus);
	CloseHandle(hMutexSetup);
	CloseHandle(hEvent);

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

void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE& hServerItem1, OPCHANDLE& hServerItem2, OPCHANDLE& hServerItem3, OPCHANDLE& hServerItem4, OPCHANDLE& hServerItem5, OPCHANDLE& hServerItem6, OPCHANDLE& hClientItem1, OPCHANDLE& hClientItem2, OPCHANDLE& hClientItem3, OPCHANDLE& hClientItem4)
{
	HRESULT hr;

	// Array of items to add:

	OPCITEMDEF ItemArray[10] =
	{{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_ID1,
		/*bActive*/ TRUE,
		/*hClient*/ 0,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},
	
	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_ID2,
		/*bActive*/ TRUE,
		/*hClient*/ 1,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_ID3,
		/*bActive*/ TRUE,
		/*hClient*/ 2,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_ID4,
		/*bActive*/ TRUE,
		/*hClient*/ 3,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_ID5,
		/*bActive*/ TRUE,
		/*hClient*/ 4,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_ID6,
		/*bActive*/ TRUE,
		/*hClient*/ 5,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_CLIENT_ID1,
		/*bActive*/ TRUE,
		/*hClient*/ 6,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_CLIENT_ID2,
		/*bActive*/ TRUE,
		/*hClient*/ 7,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_CLIENT_ID3,
		/*bActive*/ TRUE,
		/*hClient*/ 8,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	},

	{
		/*szAccessPath*/ L"",
		/*szItemID*/ ITEM_CLIENT_ID4,
		/*bActive*/ TRUE,
		/*hClient*/ 9,
		/*dwBlobSize*/ 0,
		/*pBlob*/ NULL,
		/*vtRequestedDataType*/ VT,
		/*wReserved*/0
	}};

	//Add Result:
	OPCITEMRESULT* pAddResult = NULL;
	HRESULT* pErrors = NULL;

	// Add an Item to the previous Group:
	hr = pIOPCItemMgt->AddItems(10, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK) {
		printf("Failed call to AddItems function. Error code = %x\n", hr);
		exit(0);
	}

	// Server handle for the added item:
	hServerItem1 = pAddResult[0].hServer;
	hServerItem2 = pAddResult[1].hServer;
	hServerItem3 = pAddResult[2].hServer;
	hServerItem4 = pAddResult[3].hServer;
	hServerItem5 = pAddResult[4].hServer;
	hServerItem6 = pAddResult[5].hServer;
	
	hClientItem1 = pAddResult[6].hServer;
	hClientItem2 = pAddResult[7].hServer;
	hClientItem3 = pAddResult[8].hServer;
	hClientItem4 = pAddResult[9].hServer;
	
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

void WriteItem(IUnknown* pGroupIUnknown, OPCHANDLE hServerItem, VARIANT& varValue)
{
	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**) &pIOPCSyncIO);

	// read the item value from the device:

	HRESULT* pErrors = NULL;

	HRESULT hr = pIOPCSyncIO->Write(1, &hServerItem, &varValue, &pErrors);
	if (hr != S_OK) {
		printf("Failed to send message %x.\n", hr);
		exit(0);
	}

	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}


///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//

void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE hServerItem, OPCHANDLE hServerItem2, OPCHANDLE hServerItem3, OPCHANDLE hServerItem4, OPCHANDLE hServerItem5, OPCHANDLE hServerItem6, OPCHANDLE hClientItem1, OPCHANDLE hClientItem2, OPCHANDLE hClientItem3, OPCHANDLE hClientItem4)
{
	// server handle of items to remove:
	OPCHANDLE hServerArray[10];
	hServerArray[0] = hServerItem;
	hServerArray[1] = hServerItem2;
	hServerArray[2] = hServerItem3;
	hServerArray[3] = hServerItem4;
	hServerArray[4] = hServerItem5;
	hServerArray[5] = hServerItem6;
	hServerArray[6] = hClientItem1;
	hServerArray[7] = hClientItem2;
	hServerArray[8] = hClientItem3;
	hServerArray[9] = hClientItem4;
	
	//Remove the item:
	HRESULT* pErrors; // to store error code(s)
	HRESULT hr = pIOPCItemMgt->RemoveItems(10, hServerArray, &pErrors);
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
	struct addrinfo hints;

	DWORD ret;

	int iResult,
		  iSendResult,
		  k = 0,
		  recvbuflen = DEFAULT_BUFLEN;

	char recvbuf[DEFAULT_BUFLEN];

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

				/*Numero sequencial da mensagem*/
				nseq = (((recvbuf[3] - '0') * pow(10, 6)) +
						((recvbuf[4] - '0') * pow(10, 5)) +
						((recvbuf[5] - '0') * pow(10, 4)) +
						((recvbuf[6] - '0') * pow(10, 3)) +
						((recvbuf[7] - '0') * pow(10, 2)) +
						((recvbuf[8] - '0') * pow(10, 1)) +
						(recvbuf[9] - '0') +
						1);

				nseq = nseq % 9999999;

				/*Caso tenha solicitado status da planta*/
				if (iResult == 10) {
					/*Exibe a hora corrente*/
					GetSystemTime(&SystemTime);
					printf("\x1B[31mSISTEMA DE CONTROLE: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\x1B[0m\n",
						SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
						SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);

					/*Imprime mensagem recebida em verde*/
					printf("\x1b[32m");
					printf("Msg de requisicao de dados recebida do MES\n%.10s\n\n", recvbuf);
					printf("\x1b[0m");

					ret = WaitForSingleObject(hMutexStatus, INFINITE);
					GetLastError();

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

					/*Exibe a hora corrente*/
					GetSystemTime(&SystemTime);
					printf("\x1B[31mSISTEMA DE CONTROLE: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\x1B[0m\n",
						SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
						SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);

					/*Imprime mensagem enviada em amarelo*/
					printf("\x1b[33m");
					printf("Mensagem de status da planta enviada ao MES\n%s\n\n", msgstatus);
					printf("\x1b[0m");

					ret = ReleaseMutex(hMutexStatus);
					GetLastError();
				}

				/*Caso tenha solicitado escrita no servidor OPC*/
				if (iResult == 33) {
					/*Exibe a hora corrente*/
					GetSystemTime(&SystemTime);
					printf("\x1B[31mSISTEMA DE CONTROLE: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\x1B[0m\n",
						SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
						SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);

					ret = WaitForSingleObject(hMutexSetup, INFINITE);
					GetLastError();

					/*Escrita da mensagem em uma variavel global*/
					for (int j = 0; j <= TAMMSGSETUP; j++) {
						msgsetup[j] = recvbuf[j];
					}

					ret = ReleaseMutex(hMutexStatus);
					GetLastError();

					/*Diz para o programa escrever no servidor OPC a mensagem de setup*/
					SetEvent(hEvent);
					GetLastError();

					/*Imprime mensagem recebida em cyan*/
					printf("\x1b[36m");
					printf("Mensagem de setup de equipamentos recebida do MES\n%.33s\n\n", recvbuf);
					printf("\x1b[0m");

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

					/*Exibe a hora corrente*/
					GetSystemTime(&SystemTime);
					printf("\x1B[31mSISTEMA DE CONTROLE: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\x1B[0m\n",
						SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
						SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);

					/*Imprime mensagem enviada em magenta*/
					printf("\x1b[35m");
					printf("Mensagem de confirmacao (ACK) enviada ao MES\n%s\n\n", msgack);
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
	}

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


////////////////////////////////////////////////////////////////////////
//
//
void decode() {
	char buffer[100];
	char* aux = new char;

	float var1;
	float var2;
	float var3;
	float var4;
	float var5;
	float var6;
	char* varN1 = new char;
	char* varN2 = new char;
	char* varN3 = new char;
	char* varN4 = new char;
	char* varN5 = new char;
	char* varN6 = new char;

	char msg1[4];
	char msg2[4];
	char msg3[4];
	char msg4[8];
	char msg5[4];
	char msg6[8];

	VarToStr(sdadoLeitura[0], buffer);
	var1 = atof(buffer);

	VarToStr(sdadoLeitura[1], buffer);
	var2 = atof(buffer);

	VarToStr(sdadoLeitura[2], buffer);
	var3 = atof(buffer);

	VarToStr(sdadoLeitura[3], buffer);
	var4 = atof(buffer);

	VarToStr(sdadoLeitura[4], buffer);
	var5 = atof(buffer);

	VarToStr(sdadoLeitura[5], buffer);
	var6 = atof(buffer);

	sprintf(varN1, "%03.0f", var1);
	sprintf(msg1, "%c%c%c", varN1[0], varN1[1], varN1[2]);

	sprintf(varN2, "%.0f", var2);
	sprintf(msg2, "%c%c%c", varN2[0], varN2[1], varN2[2]);

	sprintf(varN3, "%.0f", var3);
	sprintf(msg3, "%c%c%c", varN3[0], varN3[1], varN3[2]);

	sprintf(varN4, "%07.1f", var4);
	sprintf(msg4, "%c%c%c%c%c%c%c", varN4[0], varN4[1], varN4[2], varN4[3], varN4[4], varN4[5], varN4[6]);

	sprintf(varN5, "%03.0f", abs(var5));
	sprintf(msg5, "%c%c%c", varN5[0], varN5[1], varN5[2]);

	sprintf(varN6, "%07.1f", var6);
	sprintf(msg6, "%c%c%c%c%c%c%c", varN6[0], varN6[1], varN6[2], varN6[3], varN6[4], varN6[5], varN6[6]);

	sprintf(messageOPCToTCP, "%s/%s/%s/%s/%s/%s", msg4, msg6, msg1, msg2, msg3, msg5);
}