CABECALHO

	UNIVERSIDADE FEDERAL DE MINAS GERAIS

	Trabalho pratico sobre OPC e Sockets
	Sistemas distribuidos para automacao

	Professor:
	Luiz Themystokliz S. Mendes

	Alunos:
	Estevao Coelho Kiel de Oliveira
	Lucas Costa Souza

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

BIBLIOTECA WINSOCK2

	Para que o programa compile e funcione corretamente e necessario incluir a biblioteca
	Winsock2 (Ws2_32.lib) no projeto. Para isso siga os seguintes passos:

	1. No Visual Studio Community Edition va em
	Project->Properties->Configuration Properties->Linker->Input

	2. No campo Additional Dependencies adicione a biblioteca Ws2_32.lib

CREDITOS

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
