// Engenharia de Controle e Automacao - UFMG
// Disciplina: Sistemas Distribuidos para Automacao
//
// Programa-cliente para simular o Sistema ERP - Trabalho Final - 2016/1
// Luiz T. S. Mendes - 08/05/2016
//
// O código desta aplicação é baseado no "daytime client" já disponibilizado
// na página da disciplina.
//
// -------------------------------------------------------------------------
// ATENÇÃO! Apesar deste programa ter sido cuidadosamente testado pelo
// professor, ainda é possível que contenha erros. Se você encontrar algum
// erro, por favor notifique o professor por email, indicando o erro e,
// se possível, a respectiva correção.
// -------------------------------------------------------------------------
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

#include <winsock2.h>
#include <stdio.h>
#include <conio.h>

#define TAMMSGSETUP 44  // 5*8 caracteres + 4 separadores
#define TAMMSGACK   17  // 2*8 caracteres + 1 separador
#define TAMMSGREQ   17  // 2*8 caracteres + 1 separador
#define TAMMSGDADOS 44  // 5*8 caracteres + 4 separadores
#define ESC			0x1B

void main(int argc, char **argv)
{
   WSADATA     wsaData;
   SOCKET      s;
   SOCKADDR_IN ServerAddr;
   SYSTEMTIME SystemTime;
   HANDLE hTimer;
   LARGE_INTEGER Preset;
   BOOL bSucesso;
   // Define uma constante para acelerar cálculo do atraso e período
   const int nMultiplicadorParaMs = 10000;
   DWORD wstatus;
   char msgsetup[TAMMSGSETUP+1];
   char msgsetup1[TAMMSGSETUP+1] = "00000001|NNNNNNNN|00000005|00000010|07:45:00";
   char msgsetup2[TAMMSGSETUP+1] = "00000001|NNNNNNNN|00000123|00001104|23:12:15";
   char msgack[TAMMSGACK+1];
   char msgreq[TAMMSGREQ+1] = "00000005|NNNNNNNN";
   char msgdados[TAMMSGDADOS+1];
   char buf[100];

   char *ipaddr;
   int  port, status, nseqreq = 0, nseqsetup = 0, tecla, cont, vez = 0;
   
   // Testa parametros passados na linha de comando
   if (argc != 3) {
	   printf ("Uso: cliente_tcp <endereço IP> <port>\n");
	   exit(0);
   }
   ipaddr = argv[1];
   port = atoi(argv[2]);

   printf("%s : %d\n", ipaddr, port);

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

   // LOOP PRINCIPAL - cria socket e estabelece conexão com servidor TCP
   for (;;) {
     // Cria um novo socket para estabelecer a conexão.
     s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
     if (s == INVALID_SOCKET){
	     status=WSAGetLastError();
	     if ( status == WSAENETDOWN)
           printf("Rede ou servidor de sockets inacessíveis!\n");
	     else
		   printf("Falha na rede: codigo de erro = %d\n", status);
	     WSACleanup();
	     exit(0);
     }
   
	 // Estabelece a conexão inicial com o servidor
	 printf("ESTABELECENDO CONEXAO COM O SERVIDOR TCP...\n");
     status = connect(s, (SOCKADDR *) &ServerAddr, sizeof(ServerAddr));
     if (status == SOCKET_ERROR){
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
	        WSACleanup();
            exit(0);
        }
	 }

     // LOOP SECUNDARIO - Troca de mensagens com o servidor TCP, até que o usuário
     // digite ESC.
	 cont = 1;
     for (;;) {
	   // Aguarda sinalização do temporizador para enviar msg de requisição de dados
	   wstatus = WaitForSingleObject(hTimer, INFINITE);
	   if (wstatus != WAIT_OBJECT_0) {
          printf("Erro no recebimento da temporizacao ! oódigo  = %d\n", GetLastError());
	      WSACleanup();
          exit(0);
	   }
	   // Testa se usuário digitou ESC ou "s" e trata cada caso
	   if (_kbhit() != 0) {
		 tecla = _getch();
		 if (tecla == ESC) break;
		 else if (tecla == 's') {
		   // Envia msg com setup de producao
		   if (vez == 0) 
			  strncpy_s(msgsetup, sizeof(msgsetup), msgsetup1, TAMMSGSETUP);
		   else
			  strncpy_s(msgsetup, sizeof(msgsetup), msgsetup2, TAMMSGSETUP);
		   vez = 1 - vez;
	       sprintf_s(buf, 10, "%08d", ++nseqsetup);
		   memcpy_s(&msgsetup[9], 8, buf, 8);
	       status = send(s, msgsetup, TAMMSGSETUP, 0);
	       if (status == TAMMSGSETUP)
			   printf ("Msg de setup de producao enviada ao servidor TCP [%s]:\n%s\n\n", ipaddr, msgsetup);
	       else {
			   if (status == 0) 
	               // Esta situação NAO deve ocorrer, e indica algum tipo de erro.
		           printf ("SEND (1) retornou 0 bytes ... Falha!\n");
		       else if (status == SOCKET_ERROR){
                   status=WSAGetLastError();
	               printf("Falha no SEND (1): codigo de erro = %d\n", status);
			   }
		       printf ("Tentando reconexão ..\n");
			   
		       break;
		   }
	       // Aguarda mensagem de ACK do servidor TCP
	       memset(buf, sizeof(buf), 0);
	       status = recv(s, buf, TAMMSGACK, 0);
	       if (status == TAMMSGACK) {
			   strncpy_s(msgack, sizeof(msgack), buf, TAMMSGACK);
		       printf ("Msg de ACK recebida do servidor TCP [%s]:\n%s\n\n", ipaddr, msgack);
		       if (strncmp(&msgack[0], "00000002", 8) != 0) {
			       printf ("Erro na mensagem de ACK recebida!\n");
			       printf ("Msg de ACK recebida: %s\n", msgack);
			   }
	       }
	       else {
		       if (status == 0)
	              // Esta situação NÃO deve ocorrer, e indica algum tipo de erro.
		          printf ("RECV (1) retornou 0 bytes ... Falha!\n");
		       else if (status == SOCKET_ERROR){
                  status=WSAGetLastError();
	              if (status == WSAETIMEDOUT) 
					  printf ("Timeout no RECV (1)!\n");
				  else printf("Erro no RECV (1)! codigo de erro = %d\n", status);
		       }
		       printf ("Tentando reconexao ..\n");
		       break;
		   }
		 }
	   }
	   // A variavel "cont", quando igual a zero, indica o momento de enviar nova mensagem.
	   // Isto ocorre no 1o. pulso de temporização e, depois, de 20 em 20 pulsos (lembrando
	   // que 20 * 100ms = 2s).
	   if (cont == 0) {
		   // Exibe a hora corrente
           GetSystemTime(&SystemTime);
           printf ("Sistema ERP: data/hora local = %02d-%02d-%04d %02d:%02d:%02d\n",
	       SystemTime.wDay, SystemTime.wMonth, SystemTime.wYear,
		   SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond);
		   // Envia msg de requisicao
	       sprintf_s(buf, 10, "%08d", ++nseqreq);
	       memcpy_s(&msgreq[9], sizeof(msgreq), buf, 8);
	       status = send(s, msgreq, TAMMSGREQ, 0);
	       if (status == TAMMSGREQ)
			   printf ("Msg de requisicao de dados enviada ao servidor TCP [%s]:\n%s\n\n", ipaddr, msgreq);
	       else {
			   if (status == 0) 
	               // Esta situação NÂO deve ocorrer, e indica algum tipo de erro.
		           printf ("SEND (2) retornou 0 bytes ... Falha!\n");
		       else if (status == SOCKET_ERROR){
                   status=WSAGetLastError();
	               printf("Falha no SEND (2): codigo de erro = %d\n", status);
		       }
		       printf ("Tentando reconexão ..\n");
		       break;
		   }
	       // Aguarda mensagem de resposta de dados
		   printf ("ERP: Aguardando msg de ACK ...\n");
	       memset(buf, sizeof(buf), 0);
	       status = recv(s, buf, TAMMSGDADOS, 0);
	       if (status == TAMMSGDADOS) {
			   strncpy_s(msgdados, sizeof(msgdados), buf, TAMMSGDADOS);
		       printf ("Dados recebidos do servidor TCP [%s]:\n%s\n\n", ipaddr, msgdados);
		       if (strncmp(&msgdados[0], "00000010", 8) != 0)
			       printf ("Erro na mensagem de dados recebida!\n");
	       }
	       else {
		       if (status == 0)
	              // Esta situação NÃO deve ocorrer, e indica algum tipo de erro.
		          printf ("RECV (2) retornou 0 bytes ... Falha!\n");
		       else if (status == SOCKET_ERROR){
                  status=WSAGetLastError();
	              if (status == WSAETIMEDOUT) 
					  printf ("Timeout no RECV (2)!\n");
				  else printf("Erro no RECV (2)! codigo de erro = %d\n", status);
		       }
		       printf ("Tentando reconexao ..\n");
		       break;
		   }
	   }

	   // Atualiza o contador de pulsos. Quando "cont" alcanca o valor de 19, seu valor é
	   // zerado para que a proxima ativacao do temporizador (que ocorrera' no pulso de n. 20)
	   // force novo envio de mensagem.
	   cont = ++cont % 19;
	 }// end for(;;) secundário
	 if (tecla == ESC) break;
   }// end for(;;) principal

   // Fim do enlace: fecha o "socket" e encerra o ambiente Winsock 2.
   printf ("Encerrando a conexao ...\n");
   closesocket(s);
   WSACleanup();
   printf ("Conexao encerrada. Fim do programa.\n");
   system("PAUSE");
   exit(0);
}
