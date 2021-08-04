// Engenharia de Controle e Automacao - UFMG
// Disciplina: Sistemas Distribuidos para Automacao
// Trabalho final sobre OPC e Sockets
//
// Programa-cliente para simular o MES (Manufacturing Execution System) - 2021/1
// Luiz T. S. Mendes - 16/07/2021
//
// O programa envia mensagens de solicitação de dados de processo ao servidor
// de sockets, com periodicidade dada pela constante PERIODO em unidades de
// 100 ms. Assim, p. ex., se PERIODO = 20,  as mensagens serão enviadas a cada
// 20 * 100ms = 2 segundos.
//
// A tecla "s", quando digitada, força o envio de uma mensagem de setup
// de equipamentos  ao servidor de sockets.
//
// Para a correta compilacao deste programa, nao se esqueca de incluir a
// biblioteca Winsock2 (Ws2_32.lib) no projeto ! (No Visual C++ Express Edition:
// Project->Properties->Configuration Properties->Linker->Input->Additional Dependencies).
//
// Este programa deve ser executado via janela de comandos, e requer OBRIGATORIAMENTE
// o endereço IP do computador servidor e o porto de conexão. Se você quiser executá-lo
// diretamente do Visual Studio, lembre-se de definir estes dois parâmetros previamente
// clicando em Project->Properties->Configuration Properties->Debugging->Command Arguments
//
// ATENCÃO: APESAR DE TER SIDO TESTADO PELO PROFESSOR, NAO HÁ GARANTIAS DE QUE ESTE
// PROGRAMA ESTEJA LIVRE DE ERROS. SE VOCE ENCONTRAR ALGUM ERRO NO PROGRAMA, COMUNIQUE
// MESMO AO PROFESSOR, INDICANDO, SEMPRE QUE POSSÍVEL, A CORREÇÃO DO MESMO.
//

#define _CRT_SECURE_NO_WARNINGS /* Para evitar warnings sobre funçoes seguras de manipulacao de strings*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS /* para evitar warnings de funções WINSOCK depreciadas */

// Recentes versões do Visual Studio passam a indicar o seguinte
// warning do IntelliSense:
//
//    C6031: Return value ignored: 'sscanf'.
//
// Os comandos 'pragma' abaixo são para instruir o compilador a ignorar
// estes warnings. Dica extraída de:
//    https://www.viva64.com/en/k/0048/
//    https://www.fluentcpp.com/2019/08/30/how-to-disable-a-warning-in-cpp/

#pragma warning(push)
#pragma warning(disable:6031)
#pragma warning(disable:6385)
// Ao final do programa há um #pragma warning(pop) !

#include <winsock2.h>
#include <stdio.h>
#include <conio.h>

#define WHITE   FOREGROUND_RED   | FOREGROUND_GREEN     | FOREGROUND_BLUE
#define HLGREEN FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define HLRED   FOREGROUND_RED   | FOREGROUND_INTENSITY
#define HLBLUE  FOREGROUND_BLUE  | FOREGROUND_INTENSITY
#define MAGENTA FOREGROUND_RED   | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define YELLOW  FOREGROUND_RED   | FOREGROUND_GREEN

#define TAMMSGREQ    10  // 2 caracteres + separador + 7 caracteres
#define TAMMSGSTATUS 42  // 2 + 3*7 + 4*3 caracteres + separador + 7 separadores
#define TAMMSGSETUP  33  // 2 + 7 + 2 + 2*6 + 5 + 5 separadores
#define TAMMSGACK    10  // 2 caracteres + separador + 7 caracteres

#define ESC			0x1B
#define PERIODO     20  // O período é estabelecido em unidades de 100ms
                        // 20 * 100ms = 2 segundos

void main(int argc, char **argv)
{
   WSADATA       wsaData;
   SOCKET        s;
   SOCKADDR_IN   ServerAddr;
   SYSTEMTIME    SystemTime;
   HANDLE        hTimer;
   LARGE_INTEGER Preset;
   BOOL          bSucesso;

   // Define uma constante para acelerar cálculo do atraso e período
   const int nMultiplicadorParaMs = 10000;

   DWORD wstatus;
   char msgreq[TAMMSGREQ + 1] = "00/0005335";
   char msgstatus[TAMMSGSTATUS + 1];
   char msgack[TAMMSGACK + 1] = "22/0045673";
   char msgsetup1[TAMMSGSETUP + 1] = "77/0127668/02/0050.0/0040.0/00050";
   char msgsetup2[TAMMSGSETUP + 1] = "77/0003457/01/0050.0/0073.0/00105";
   char msgsetup[TAMMSGSETUP + 1];
   char buf[100];
   char msgcode[3];
   char *ipaddr;
   int  port, status, nseq = 0, nseqreq = 0, tecla = 0, cont, vez = 0, flagreq = 0;
   int nseql, nseqr;
   int k=0;
   HANDLE hOut;

   // Testa parametros passados na linha de comando
   if (argc != 3) {
	   printf ("Uso: cliente_tcp <endereço IP> <port>\n");
	   exit(0);
   }
   ipaddr = argv[1];
   port = atoi(argv[2]);

   // Inicializa Winsock versão 2.2
   status = WSAStartup(MAKEWORD(2,2), &wsaData);
   if (status != 0) {
	   printf("Falha na inicializacao do Winsock 2! Erro  = %d\n", WSAGetLastError());
	   WSACleanup();
       exit(0);
   }

   // Cria "waitable timer" com reset automático
   hTimer = CreateWaitableTimer(NULL, FALSE, "MyTimer");

   // Programa o temporizador para que a primeira sinalização ocorra 100ms 
   // depois de SetWaitableTimer 
   // Use - para tempo relativo
   Preset.QuadPart = -(100 * nMultiplicadorParaMs);
   // Dispara timer
   bSucesso = SetWaitableTimer(hTimer, &Preset, 100, NULL, NULL, FALSE);
   if(!bSucesso) {
	 printf ("Erro no disparo da temporização! Código = %d\n", GetLastError());
	 WSACleanup();
	 exit(0);
   }

   // Inicializa a estrutura SOCKADDR_IN que será utilizada para
   // a conexão ao servidor.
   ServerAddr.sin_family = AF_INET;
   ServerAddr.sin_port = htons(port);    
   ServerAddr.sin_addr.s_addr = inet_addr(ipaddr);

   	// Obtém um handle para a saída da console
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
		printf("Erro ao obter handle para a saída da console\n");

   // ==================================================================
   // LOOP PRINCIPAL - CRIA SOCKET E ESTABELECE CONEXÃO COM O SERVIDOR
   // ==================================================================

   while (TRUE) {
     //printf("LOOP PRINCIPAL\n");
	 SetConsoleTextAttribute(hOut, WHITE);
	 nseql = 0; // Reinicia numeração das mensagens

     // Cria um novo socket para estabelecer a conexão.
     s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
     if (s == INVALID_SOCKET) {
		 SetConsoleTextAttribute(hOut, WHITE);
	     status=WSAGetLastError();
	     if ( status == WSAENETDOWN)
           printf("Rede ou servidor de sockets inacessíveis!\n");
	     else
		   printf("Falha na rede: codigo de erro = %d\n", status);
		 SetConsoleTextAttribute(hOut, WHITE);
	     WSACleanup();
	     exit(0);
     }
   
	 // Estabelece a conexão inicial com o servidor
	 printf("ESTABELECENDO CONEXAO COM O SERVIDOR TCP...\n");
     status = connect(s, (SOCKADDR *) &ServerAddr, sizeof(ServerAddr));
     if (status == SOCKET_ERROR) {
		SetConsoleTextAttribute(hOut, HLRED);
	    if (WSAGetLastError() == WSAEHOSTUNREACH) {
			printf ("Rede inacessivel... aguardando 5s e tentando novamente\n");
			// Testa se ESC foi digitado antes da espera
		    if (_kbhit() != 0) {
		      if (_getch() == ESC) break;
			}
			else { 
			  Sleep(5000);
			  continue;
			}
		}
		else {
			printf("Falha na conexao ao servidor ! Erro  = %d\n", WSAGetLastError());
			SetConsoleTextAttribute(hOut, WHITE);
	        WSACleanup();
			closesocket(s);
            exit(0);
        }
	 }

	 // ----------------------------------------------------------------------
     // LOOP SECUNDARIO - Troca de mensagens com o Sistema de controle,
     // até que o usuário digite ESC.
	 // ----------------------------------------------------------------------

	 cont = 0;
     for (;;) {
	   // Aguarda sinalização do temporizador para enviar msg de requisição de dados
	   wstatus = WaitForSingleObject(hTimer, INFINITE);
	   if (wstatus != WAIT_OBJECT_0) {
		  SetConsoleTextAttribute(hOut, HLRED);
		  printf("Erro no recebimento da temporizacao ! código  = %d\n", GetLastError());
	      WSACleanup();
		  closesocket(s);
          exit(0);
	   }
	   // Testa se usuário digitou ESC, em caso afirmativo, encerra o programa
	   if (_kbhit() != 0)
		 if ((tecla = _getch()) == ESC) break;
	     // Ajusta indicador caso a tecla 's' tenha sido digitada
		 else if (tecla == 's') flagreq = 1;
	   
	   // A variavel "cont", quando igual a zero, indica o momento de enviar nova mensagem.
	   // Isto ocorre no 1o. pulso de temporização e, depois, de 20 em 20 pulsos (lembrando
	   // que 20 * 100ms = 2s).
	   if (cont == 0) {
		 // Exibe a hora corrente
         GetSystemTime(&SystemTime);
		 SetConsoleTextAttribute(hOut, WHITE);
		 printf ("MES: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\n",
	              SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
		          SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);
		 
		 // Envia msg de solicitação de dados
	     sprintf(buf, "%07d", ++nseql);
	     memcpy(&msgreq[3], buf, 7);
	     status = send(s, msgreq, TAMMSGREQ, 0);
	     if (status == TAMMSGREQ) {
           SetConsoleTextAttribute(hOut, HLGREEN);
		   printf ("Msg de requisicao de dados enviada ao sistema de controle [%s]:\n%s\n\n",
			       ipaddr, msgreq);
		 }
		 else {
		   SetConsoleTextAttribute(hOut, HLRED);
           if (status == 0)
			 // Esta situação NÂO deve ocorrer, e indica algum tipo de erro.
			 printf("SEND (2) retornou 0 bytes ... Falha!\n");
		   else if (status == SOCKET_ERROR) {
			 status = WSAGetLastError();
			 printf("Falha no SEND (2): codigo de erro = %d\n", status);
		   }
		   printf("Tentando reconexão ..\n\n");
		   break;
		 }

		 // Aguarda mensagem de status da planta
	     memset(buf, sizeof(buf), 0);
	     status = recv(s, buf, TAMMSGSTATUS, 0);
	     if (status == TAMMSGSTATUS) {
		   if (strncmp(buf, "99", 2) != 0) {
			 strncpy(msgcode, buf, 2);
			 msgcode[2] = 0x00;
			 SetConsoleTextAttribute(hOut, HLRED);
			 printf("Codigo incorreto de mensagem de status da planta: recebido '%s' ao inves de '99'\n", msgcode);
			 printf("Encerrando o programa...\n");
			 closesocket(s);
			 WSACleanup();
			 SetConsoleTextAttribute(hOut, WHITE);
			 exit(0);
		   }
		   sscanf(&buf[3], "%7d", &nseqr);
		   if (++nseql != nseqr) {
			 SetConsoleTextAttribute(hOut, HLRED);
			 printf("Numero sequencial de mensagem incorreto [1]: recebido %d ao inves de %d.\n",
			         nseqr, nseql);
			 printf("Encerrando o programa...\n");
			 closesocket(s);
			 SetConsoleTextAttribute(hOut, WHITE);
			 WSACleanup();
			 exit(0);
		   }
		   strncpy(msgstatus, buf, TAMMSGSTATUS);
		   msgstatus[TAMMSGSTATUS] = 0x00;
		   SetConsoleTextAttribute(hOut, YELLOW);
		   printf ("Mensagem de status da planta recebida do sistema de controle [%s]:\n%s\n\n",
			       ipaddr, msgstatus);
		 }
	     else {
		   SetConsoleTextAttribute(hOut, HLRED);
		   if (status == 0)
	         // Esta situação NÃO deve ocorrer, e indica algum tipo de erro.
		     printf ("RECV (2) retornou 0 bytes ... Falha!\n");
		   else if (status == SOCKET_ERROR) {
             status = WSAGetLastError();
	         if (status == WSAETIMEDOUT) 
			   printf ("Timeout no RECV (2)!\n");
		     else printf("Erro no RECV (2)! codigo de erro = %d\n", status);
		   }
		   printf ("Tentando reconexao ..\n\n");
		   break;
		   }
	     }

	   // Testa se usuário digitou tecla "s"
	   if (flagreq == 1) {
		 flagreq = 0;

		 // Envia msg de setup de equipamentos ao sistema de controle
		 if (vez == 0)
			 strncpy(msgsetup, msgsetup1, TAMMSGSETUP);
		 else
			 strncpy(msgsetup, msgsetup2, TAMMSGSETUP);
		 vez = 1 - vez;
		 sprintf(buf, "%07d", ++nseql);
		 memcpy(&msgsetup[3], buf, 7);
	     status = send(s, msgsetup, TAMMSGSETUP, 0);
		 if (status == TAMMSGSETUP){
		   msgsetup[TAMMSGSETUP] = 0x00;
		   SetConsoleTextAttribute(hOut, HLBLUE);
		   printf("Mensagem de setup de equipamentos enviada ao sistema de controle [%s]:\n%s\n\n",
			       ipaddr, msgsetup);
		 }
		 else {
		   SetConsoleTextAttribute(hOut, HLRED);
		   if (status == 0)
			 // Esta situação NÃO deve ocorrer, e indica algum tipo de erro.
		     printf("SEND (3) retornou 0 bytes ... Falha!\n");
		   else if (status == SOCKET_ERROR) {
			 status = WSAGetLastError();
			 printf("Falha no SEND (3): codigo de erro = %d\n", status);
		   }
		   printf("Tentando reconexão ..\n\n");
		   break;
		 }

		 // Aguarda mensagem de ACK do Sistema de controle
		 memset(buf, sizeof(buf), 0);
		 status = recv(s, buf, TAMMSGACK, 0);
		 if (status == TAMMSGACK) {
		   if (strncmp(buf, "22", 2) != 0) {
		     strncpy(msgcode, buf, 2);
			 msgcode[2] = 0x00;
			 SetConsoleTextAttribute(hOut, HLRED);
			 printf("Codigo incorreto de mensagem de ACK: recebido '%s' ao inves de '22'\n", msgcode);
			 printf("Encerrando o programa...\n");
			 closesocket(s);
			 WSACleanup();
			 SetConsoleTextAttribute(hOut, WHITE);
			 exit(0);
		   }
		   sscanf(&buf[3], "%7d", &nseqr);
		   if (++nseql != nseqr) {
			 SetConsoleTextAttribute(hOut, HLRED);
			 printf("Numero sequencial de mensagem incorreto [1]: recebido %d ao inves de %d.\n",
				     nseqr, nseql);
			 printf("Encerrando o programa...\n");
			 closesocket(s);
			 WSACleanup();
			 SetConsoleTextAttribute(hOut, WHITE);
			 exit(0);
		   }
		   strncpy(msgack, buf, TAMMSGACK);
		   msgack[TAMMSGACK] = 0x00;
		   SetConsoleTextAttribute(hOut, MAGENTA);
		   printf("Mensagem de ACK recebida do sistema de controle [%s]:\n%s\n\n", ipaddr, msgack);
		 }
		 else {
		   SetConsoleTextAttribute(hOut, HLRED);
		   if (status == 0)
		     // Esta situação NÃO deve ocorrer, e indica algum tipo de erro.
			 printf("RECV (4) retornou 0 bytes ... Falha!\n");
		   else if (status == SOCKET_ERROR) {
		     status = WSAGetLastError();
			 if (status == WSAETIMEDOUT) printf("Timeout no RECV (4)!\n");
			 else printf("Erro no RECV (4)! codigo de erro = %d\n", status);
		   }
		   printf("Tentando reconexao ..\n\n");
	       break;
		 }
	   }
	   // Atualiza o contador de pulsos. Quando "cont" alcanca o valor de 19, seu valor é
	   // zerado para que a proxima ativacao do temporizador (que ocorrera' no pulso de n. 20)
	   // force novo envio de mensagem.
	   cont = ++cont % (PERIODO-1);
	 }// end for(;;) secundário

	 if (tecla == ESC) break;
	 else closesocket(s);
   }// end while principal

   // Fim do enlace: fecha o "socket" e encerra o ambiente Winsock 2.
   SetConsoleTextAttribute(hOut, WHITE);
   printf ("Encerrando a conexao ...\n");
   closesocket(s);
   WSACleanup();
   printf ("Conexao encerrada. Fim do programa.\n");
   exit(0);
}
#pragma warning(pop)
