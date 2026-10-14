# Blynk: comando remoto e feedback lógico

[Voltar ao README](../README.md)

O Blynk é a interface de operação escolhida: envia ON/OFF e recebe o estado que o firmware aplicou. O Wokwi simula o ESP32, mas a conexão com o Blynk usa um serviço real na internet.

## Configurar o painel

1. No Blynk Console, criar um Template chamado **Smart Light**, com hardware ESP32 e conexão Wi-Fi.
2. Criar os dois datastreams de pino virtual da tabela abaixo.
3. Associar um Switch a V0, com OFF=0 e ON=1. Usar comutação que mantenha o estado, em vez de um botão momentâneo.
4. Associar um indicador de valor a V1. Se usar um widget LED, configurar sua escala para 0–1; o código não envia 255.
5. Criar um Device a partir desse Template. Usar o Template ID e o Auth Token desse dispositivo na **cópia privada** do firmware, conforme o [README](../README.md#configurar-credenciais).
6. Salvar o painel. Se houver painel web e painel móvel, conferir os widgets e datastreams em cada um.

| Datastream | Tipo | Mínimo / máximo | Valor inicial recomendado | Papel no projeto |
| --- | --- | --- | --- | --- |
| V0 — Comando | Integer | 0 / 1 | 0 | Blynk → ESP32 |
| V1 — Estado lógico | Integer | 0 / 1 | 0 | ESP32 → Blynk |

Template é o modelo de configuração. Device é a instância que se autentica. Datastream transporta um valor; widget apresenta ou altera esse valor. O Auth Token autoriza o dispositivo e deve permanecer privado. O Template ID identifica o modelo e precisa corresponder ao Device.

V0 e V1 são canais de software, não GPIOs. O valor 1 não significa 1 volt. Suas direções são escolhas deste firmware; a API de pinos virtuais permite envio e recebimento. [Referência de pinos virtuais](https://docs.blynk.io/en/blynk-library-firmware-api/virtual-pins).

## O que o código faz

`BLYNK_WRITE(V0)` recebe `param.asInt()`, rejeita inteiros diferentes de 0 e 1, chama `aplicarEstado()` e depois `enviarFeedback()`. O retorno usa `Blynk.virtualWrite(V1, lampadaLigada ? 1 : 0)` quando há conexão. Não há um callback de comando em V1.

`aplicarEstado()` contabiliza primeiro o tempo do estado anterior, soma um acionamento somente em OFF → ON e escreve GPIO18. Na configuração preservada, HIGH corresponde a ligado e LOW a desligado. Um segundo ON, sem OFF intermediário, não aumenta a contagem.

Ao conectar ou reconectar, `BLYNK_CONNECTED()` solicita `Blynk.syncVirtual(V0)`. A resposta passa pelo mesmo callback, aplica o comando salvo e envia V1. Isso pode ligar novamente a saída após um reinício, mesmo que `setup()` a tenha iniciado desligada. A sincronização explícita de V0 não depende de habilitar a opção usada por `syncAll()`. [Sincronização de estado](https://docs.blynk.io/en/blynk-library-firmware-api/state-syncing).

V1 confirma a decisão lógica do firmware. Não mede luminosidade, corrente ou continuidade do circuito; uma carga desconectada ainda pode coexistir com V1=1. Online/Offline informa conexão. Um valor antigo no widget não comprova que o dispositivo continua online.

## Conexão e diagnóstico

O firmware inclui `BlynkSimpleEsp32_SSL.h` e usa `Blynk.config()`. A rotina `manterConexoes()` chama `Blynk.run()` quando conectado e tenta `Blynk.connect(30000)` quando desconectado. Há pelo menos 10 s desde o fim da última tentativa antes de iniciar outra, sujeito ao andamento do loop. DNS/TLS podem acrescentar espera; isso não constitui controle totalmente não bloqueante. [Gerenciamento da conexão](https://docs.blynk.io/en/blynk-library-firmware-api/connection-management).

| Sintoma | Verificação |
| --- | --- |
| Mensagem para preencher token | O placeholder `COLE_` ainda está presente na cópia de execução. |
| WiFi=OFF | A simulação está executando e consegue acessar a rede Wokwi-GUEST? |
| WiFi=ON, Blynk=OFF | Conferir token novo, Device, mensagens de autenticação/TLS e resultado posterior da tentativa. |
| V0 muda no painel, sem callback | Conferir Device online e associação do widget a V0. |
| GPIO acompanha, V1 não aparece | Conferir datastream e indicador 0–1 de V1, além da conexão. |
| LED aparenta estado invertido | Repetir teste local e conferir diagrama, IN e NO; não inverter código por tentativa sem evidência. |

O NTP é iniciado após a conexão Wi-Fi, mas o caminho Blynk não aguarda explicitamente a confirmação de horário válido. Isso é um ponto de investigação de TLS, não uma causa demonstrada da última desconexão.

## Segurança e estado observado

A inspeção de 14/10/2026 identificou um Auth Token literal no projeto público de origem. É necessário invalidá-lo e gerar uma credencial nova pelo gerenciamento do Device. Retirar a string do GitHub não revoga uma credencial já divulgada. Conferir também a cópia publicada no Wokwi e eventuais cópias compartilhadas.

O material do grupo, datado de 11/10/2026, registra conexão e comandos bem-sucedidos, mas termina em tentativa de reconexão e painel Offline. Não há evidência posterior de recuperação. O procedimento para fechar essa pendência está em [testes](testes.md).

Referência de configuração: [Blynk — datastreams](https://docs.blynk.io/en/blynk.console/templates/datastreams/datastreams-common-settings.md).
