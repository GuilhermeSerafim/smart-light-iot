# Firmware Smart Light: funcionamento e revisão técnica

Revisão do firmware, de `diagram.json` e de `libraries.txt`, realizada em 14/10/2026 a partir de uma cópia sanitizada do código. Nenhuma alteração funcional do firmware foi feita nesta revisão. Os resultados relatados pelo responsável foram: controle Blynk funcionando e envio ThingSpeak com HTTP 200 e `entry_id` positivo. A recuperação após a última desconexão ainda precisa de evidência de execução; a existência de código de reconexão não comprova esse resultado.

## Plataforma, circuito e contrato da saída

O programa utiliza ESP32 com Arduino e FreeRTOS. O enunciado mencionado nos comentários usa ESP8266; a substituição precisa estar aceita pelo professor. A troca para ESP8266 exigiria adaptar bibliotecas, pinagem e tarefas: o sketch atual é específico de ESP32.

O diagrama contém ESP32 DevKitC V4, módulo de relé com `transistor="npn"`, resistor de 220 Ω e LED amarelo identificado como lâmpada. As ligações de controle são GPIO18 → IN, 5 V → VCC e GND → GND. No circuito da carga: 5 V → COM, NO → resistor → ânodo do LED, cátodo → GND. O monitor serial está conectado a TX/RX. Não há sensor de corrente, potência ou luminosidade.

O contrato do firmware fornecido é **GPIO18 HIGH = ligado; GPIO18 LOW = desligado**. A [documentação do relé Wokwi](https://docs.wokwi.com/parts/wokwi-relay-module) apresenta informações conflitantes entre a tabela do transistor NPN e a descrição operacional. Essa divergência documental não permite concluir que há erro de polaridade no projeto. O modo local permite verificar a correspondência visual no simulador; este documento preserva o contrato existente.

O LED representa uma carga de baixa tensão para demonstração. O diagrama não é um projeto de instalação elétrica de lâmpada em rede.

## Configuração e dependências

| Item | Origem e função |
| --- | --- |
| `Arduino.h` | Núcleo Arduino: GPIO, `millis()`, `String`, serial e `delay()` |
| `WiFi.h` | Núcleo Arduino-ESP32: conexão à rede e estado do Wi-Fi |
| `WiFiClientSecure.h` | Núcleo Arduino-ESP32: cliente TLS e certificados |
| `HTTPClient.h` | Núcleo Arduino-ESP32: requisição HTTPS ao ThingSpeak |
| `time.h` | Consulta ao relógio com `time()`; `configTime()` é uma extensão fornecida pelo Arduino-ESP32 |
| `stdlib.h` | Conversão do identificador de entrada com `strtol()` |
| `freertos/FreeRTOS.h`, `task.h`, `queue.h` | Tarefa e fila fornecidas pela plataforma ESP32 |
| `BlynkSimpleEsp32_SSL.h` | Biblioteca externa Blynk, variante TLS para ESP32 |

`libraries.txt` declara somente `Blynk`, sem versão fixada. A versão do núcleo Arduino-ESP32 também não está especificada nos três arquivos analisados. Portanto, a reprodução exata depende das versões usadas pelo ambiente de compilação. Não é necessário adicionar uma biblioteca ThingSpeak: o código usa diretamente a API HTTPS.

O modelo público deve manter `COLE_SEU_BLYNK_TEMPLATE_ID`, `COLE_SEU_BLYNK_AUTH_TOKEN` e `COLE_SUA_THINGSPEAK_WRITE_API_KEY` como placeholders. O nome de template é Smart Light. Token e Write API Key pertencem à configuração privada; o Template ID identifica o template e não substitui o token de autenticação. Prepare uma cópia privada em `local/`, diretório ignorado pelo Git conforme a organização do projeto, e preencha nela os valores reais. Não há dependência de `secrets.h` no firmware analisado.

O teste de placeholder presente no sketch verifica prefixo `COLE_`; isso evita começar com o modelo sem configurar. Não valida se um token/chave arbitrário, vazio ou incorreto é utilizável. Um token Blynk com esse prefixo encerra a configuração de nuvem antes de iniciar Wi-Fi e ThingSpeak. Uma chave ThingSpeak com esse prefixo desativa somente a preparação da telemetria. `ATIVAR_THINGSPEAK=false` também desativa somente essa preparação.

## Constantes e tempos

| Parâmetro | Valor | Efeito |
| --- | --- | --- |
| `TESTE_LOCAL` | `false` | Executa o modo de nuvem por padrão |
| `ATIVAR_THINGSPEAK` | `true` | Habilita a preparação da tarefa de telemetria |
| `PIN_RELE` | 18 | GPIO conectado ao IN do módulo |
| `RELE_LIGADO` / `RELE_DESLIGADO` | `HIGH` / `LOW` | Contrato lógico da saída |
| `POTENCIA_LAMPADA_W` | 9,0 W | Hipótese usada no cálculo de energia |
| `INTERVALO_ENVIO_MS` | 30.000 ms | Intervalo mínimo entre amostras produzidas pelo loop |
| `INTERVALO_STATUS_MS` | 5.000 ms | Intervalo mínimo para imprimir status |
| Alternância local | 2.000 ms | Troca o estado no modo local |
| Tentativa explícita de Wi-Fi | 15.000 ms | Chama `WiFi.reconnect()` enquanto desconectado |
| Tentativa explícita de Blynk | 10.000 ms | Espera após o término da última tentativa |
| `Blynk.connect()` | 30.000 ms | Prazo solicitado ao protocolo, sem garantia de limite total DNS/TLS |
| Handshake TLS ThingSpeak | 8 s | Prazo configurado no cliente seguro |
| Conexão HTTP | 4.000 ms | Prazo configurado em `HTTPClient` |
| Leitura HTTP | 5.000 ms | Prazo configurado em `HTTPClient` |
| Pausa da tarefa ThingSpeak | 20.000 ms | Descanso após cada amostra consumida, inclusive uma tentativa sem sucesso |
| `delay(1)` | 1 ms solicitado | Cede execução ao sistema ao final do loop |
| Serial | 115.200 baud | Logs do firmware |
| Rede simulada | `Wokwi-GUEST`, senha vazia, canal 6 | Configuração de `WiFi.begin()` |
| Relógio mínimo para ThingSpeak | Unix 1.704.067.200 | 01/01/2024 UTC; barreira simples antes do TLS |
| NTP | `pool.ntp.org`, `time.google.com` | Configurado com offsets UTC iguais a zero |
| Fila | 1 `Amostra` | Mantém a amostra pendente mais recente |
| Tarefa `thingspeak` | pilha 12.288 bytes, prioridade 1 | Criada com `xTaskCreate()`, sem fixar núcleo |

Os intervalos do loop são mínimos condicionais, não um calendário exato. Uma operação síncrona demorada pode adiar status, amostragem e tentativas seguintes. Os prazos HTTP/TLS referem-se a etapas; não devem ser somados e apresentados como um limite global garantido.

Na versão FreeRTOS do ESP-IDF, o tamanho de pilha usado em `xTaskCreate()` é expresso em bytes, diferentemente de algumas distribuições FreeRTOS. Ver [documentação do ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html).

## Mapa das funções

| Função ou callback | Responsabilidade |
| --- | --- |
| `atualizarTempo()` | Calcula o tempo decorrido desde a última atualização; soma-o ao total somente se a lâmpada estava ligada; atualiza a referência de tempo |
| `aplicarEstado(bool)` | Contabiliza o intervalo anterior, incrementa acionamentos em transição desligado → ligado, grava o estado lógico e atualiza GPIO18 |
| `enviarFeedback()` | Publica 0 ou 1 em V1 quando Blynk está conectado |
| `BLYNK_WRITE(V0)` | Obtém um inteiro, rejeita valores diferentes de 0/1, aplica o comando, publica V1 e imprime o resultado |
| `BLYNK_CONNECTED()` | Solicita ao servidor o último V0 com `Blynk.syncVirtual(V0)` |
| `imprimirStatus()` | Imprime modo, Wi-Fi, Blynk, estado lógico, leitura GPIO18, horas e acionamentos |
| `manterConexoes()` | Detecta ausência de Wi-Fi, desconecta Blynk quando necessário, tenta Wi-Fi periodicamente, inicia NTP uma vez e executa ou reconecta Blynk |
| `enviarThingSpeak(...)` | Verifica Wi-Fi/relógio, configura cliente TLS, envia POST, avalia resposta, imprime diagnóstico e encerra os clientes |
| `tarefaThingSpeak(void*)` | Espera uma amostra na fila, envia seus valores e suspende somente sua própria execução por 20 s |
| `prepararThingSpeak()` | Verifica habilitação/chave, cria fila e tarefa; trata falhas de alocação sem impedir o Blynk |
| `setup()` | Inicializa serial e relé desligado; seleciona modo local ou configura Wi-Fi, Blynk e telemetria |
| `loop()` | Atualiza tempo; alterna no teste local ou mantém conexões e produz amostras; imprime status; cede CPU |

`Amostra` é uma estrutura de dados, não uma função. Contém `estado` inteiro, `horas` double, `acionamentos` unsigned long e `energiaKWh` double. `THINGSPEAK_CA` é o conjunto público de certificados usado pelo cliente ThingSpeak.

## Algoritmo de tempo, acionamentos e energia

O estado inicial em RAM é desligado, com acionamentos e tempo acumulado iguais a zero. Antes de alterar o estado, `aplicarEstado()` chama `atualizarTempo()`. Assim, o intervalo que acabou de transcorrer é atribuído ao estado anterior.

O cálculo central é:

```text
agora = millis() convertido para uint32_t
decorrido = agora - ultimaMedicaoMs, em aritmética uint32_t
se lampadaLigada: tempoLigadaMs += decorrido
ultimaMedicaoMs = agora
```

A subtração sem sinal funciona na passagem de `millis()` por zero, desde que o intervalo entre atualizações seja inferior a um ciclo completo de 32 bits: aproximadamente 49,7 dias. O mesmo princípio é usado nos agendamentos. O acumulador é `uint64_t`, evitando limitar o total de tempo ligado a esse ciclo. O código não deve ser descrito como imune a qualquer paralisação ou a múltiplos ciclos sem atualização.

Acionamentos contam **transições de desligado para ligado**. Repetir V0=1 enquanto já ligado não incrementa; desligar também não incrementa. Ao reconectar após reset, receber V0=1 liga a saída e conta um acionamento da nova sessão.

```text
horas = tempoLigadaMs / 3.600.000
energia_kWh = (9,0 / 1.000) × horas
```

Uma hora ligada corresponde, pela hipótese adotada, a 0,009 kWh. Não há medição física. O tempo mede o estado lógico comandado, sem confirmar que relé, LED ou uma carga externa realmente funcionaram. `digitalRead(GPIO18)` lê o nível do pino e também não confirma o contato do relé ou consumo.

As variáveis vivem apenas em RAM: reinício ou interrupção da simulação zera o histórico local. Dados já aceitos pelo ThingSpeak continuam no serviço; o firmware não os consulta para restaurar contadores. No ESP32, `unsigned long` é de 32 bits e o contador de acionamentos também pode dar rollover após seu limite.

## Fluxo de execução e FreeRTOS

1. `setup()` configura a saída e a escreve em LOW. Inicializa as referências de medição e teste.
2. Com `TESTE_LOCAL=true`, retorna antes de iniciar as nuvens. O loop alterna a saída a cada 2 s e mantém logs/contagem.
3. No modo de nuvem com configuração preenchida, inicia Wi-Fi em estação, habilita reconexão automática e configura Blynk. A primeira tentativa Blynk é preparada para ocorrer assim que houver Wi-Fi.
4. A preparação da telemetria cria uma fila de capacidade 1. Se a criação de fila ou tarefa falhar, imprime o erro; na falha da tarefa, apaga a fila e deixa o handle nulo.
5. O loop continua responsável por GPIO, estado, cálculo acumulado e chamadas da API Blynk. A tarefa HTTP não altera essas variáveis.
6. Com Wi-Fi conectado e intervalo vencido, o loop calcula os quatro campos e usa `xQueueOverwrite()` para copiar a estrutura para a fila.
7. A tarefa espera com `xQueueReceive(..., portMAX_DELAY)`. Ao receber, trabalha com sua própria cópia, envia o POST e dorme por 20 s.

Como a fila copia a estrutura, a tarefa não depende de ponteiros para variáveis locais do loop nem lê seus contadores durante alterações. No máximo há uma amostra em processamento e uma pendente; sobrescrever a pendente descarta a anterior. Não há armazenamento offline, lista de reenvios nem confirmação devolvida ao loop. Uma tentativa que falha também consome a amostra. A próxima produção normal levará os totais acumulados mais recentes, mas não recompõe todos os estados intermediários.

A separação evita que o POST e sua pausa de 20 s bloqueiem diretamente o processamento Blynk. A tarefa não está presa a um núcleo específico; portanto, não se deve afirmar que cada serviço ocupa necessariamente um núcleo diferente. Há recursos comuns da plataforma, como Wi-Fi, heap e serial, e duas conexões TLS podem coexistir. A revisão não mediu uso de pilha/heap nem encontrou evidência de esgotamento.

## Blynk, estado e reconexão

V0 é o comando e V1 é o feedback do estado lógico aplicado. Para uso previsível, configure datastreams inteiros de 0 a 1; use um controle de estado em V0 e um indicador em V1. O callback rejeita inteiros fora desse domínio. O feedback é enviado quando um comando válido é aplicado, inclusive um comando recebido por sincronização; não existe publicação periódica independente em V1.

Ao conectar, `BLYNK_CONNECTED()` pede o último valor de V0. A resposta chega por `BLYNK_WRITE(V0)` e passa pelo mesmo caminho de aplicação/feedback. A [documentação oficial de sincronização](https://docs.blynk.io/en/blynk-library-firmware-api/state-syncing) informa que `syncVirtual()` solicita os pinos indicados sem depender da opção de sincronizar todos os datastreams. Um valor ainda não inicializado pode não produzir resposta.

Se a rede cair, o firmware mantém o último estado lógico e a saída. Não há desligamento automático por perda de Wi-Fi ou Blynk. O tempo continua sendo acumulado no loop. Sem Wi-Fi, as novas amostras não são enfileiradas; a tarefa também verifica a rede antes de enviar. Se somente Blynk cair e o Wi-Fi continuar funcional, a telemetria ainda pode ser produzida.

O Wi-Fi usa tanto reconexão automática quanto chamadas explícitas a cada 15 s quando desconectado. Quando há Wi-Fi, Blynk recebe `run()` se já está conectado; caso contrário, a tentativa explícita é feita após 10 s desde o término da anterior. O `Blynk.connect(30000)` continua síncrono no loop. Durante essa operação, status, novos comandos processados pelo loop e criação de amostras podem atrasar. A atualização de tempo imediatamente depois inclui o intervalo transcorrido, desde que a execução retorne normalmente.

Após reset, a saída começa desligada e os totais zerados. Depois da conexão, o V0 armazenado na nuvem pode ligar novamente a saída. Após reconexão sem reset, o V0 recebido pode reafirmar ou mudar o estado preservado localmente. Esse comportamento restaura o comando desejado na nuvem, não o histórico local.

A última recuperação de conexão não foi comprovada nesta revisão. Sem registro serial e teste controlado, não é possível atribuir uma desconexão a token, TLS, relógio, Wi-Fi, servidor ou memória.

## HTTPS, relógio e certificados

O NTP é configurado na primeira passagem com Wi-Fi conectado. `iniciouNtp` indica que a configuração foi solicitada, e não que a sincronização já terminou. O serviço da plataforma administra a sincronização depois dessa configuração; o sketch não implementa acompanhamento detalhado de sua qualidade.

Antes do envio ThingSpeak, `time(nullptr)` precisa ser pelo menos 01/01/2024 UTC. Isso impede iniciar essa conexão com um relógio claramente não inicializado, mas não verifica sua precisão ou estabelece um limite superior. No caminho Blynk, o código solicita NTP e pode tentar TLS na mesma passagem, sem aguardar explicitamente essa barreira. Uma primeira tentativa pode ocorrer antes da sincronização. Isso é uma possibilidade do fluxo, não uma causa comprovada de falha.

O ThingSpeak usa `WiFiClientSecure`, `setCACert(THINGSPEAK_CA)` e a URL `https://api.thingspeak.com/update`. Não há `setInsecure()` no sketch. O Blynk usa a variante SSL e seus certificados padrão de biblioteca, independentes de `THINGSPEAK_CA`. O [código oficial atual do adaptador Blynk](https://github.com/blynkkk/blynk-library/blob/master/src/BlynkSimpleEsp32_SSL.h) configura CA e timeout de handshake de 30 s; a reprodução deve conferir a versão efetivamente compilada, pois `libraries.txt` não a fixa.

Os cinco certificados embutidos no sketch são públicos. Os nomes e períodos abaixo foram extraídos dos blocos PEM da cópia sanitizada; não são credenciais:

| CA raiz | Início de validade UTC | Fim de validade UTC |
| --- | --- | --- |
| DigiCert High Assurance EV Root CA | 10/11/2006 | 10/11/2031 |
| DigiCert Global Root CA | 10/11/2006 | 10/11/2031 |
| DigiCert Global Root G2 | 01/08/2013 | 15/01/2038 |
| ISRG Root X1 | 04/06/2015 | 04/06/2035 |
| Amazon Root CA 1 | 26/05/2015 | 17/01/2038 |

Na data da revisão, os períodos incluem a data atual. Isso não comprova, sozinho, que a cadeia apresentada pelo servidor em uma conexão específica será aceita: também dependem o certificado do servidor, hostname, cadeia intermediária, relógio e implementação TLS. Uma mudança futura de cadeia pode exigir manutenção das raízes. A lista contém alternativas de confiança; não há seleção explícita de uma única CA por envio.

## ThingSpeak e interpretação da resposta

| Campo | Conteúdo | Unidade/interpretação |
| --- | --- | --- |
| `field1` | Estado | 0 desligado, 1 ligado, estado lógico no instante da amostra |
| `field2` | Tempo ligado | Horas acumuladas desde o boot, seis casas decimais |
| `field3` | Acionamentos | Número de transições desligado → ligado desde o boot |
| `field4` | Energia estimada | kWh, oito casas decimais, hipótese de 9 W |

A chave de escrita é enviada no cabeçalho `THINGSPEAKAPIKEY`. O corpo usa `application/x-www-form-urlencoded`. A API oficial aceita essa combinação, conforme [Write Data, da MathWorks](https://www.mathworks.com/help/thingspeak/writedata.html).

O sucesso exige simultaneamente HTTP 200 e corpo convertido integralmente para um inteiro positivo. Resposta `0` significa recusa da gravação; HTTP 200 isolado não basta. Códigos negativos geram diagnóstico HTTP/TLS; outros resultados não são anunciados como sucesso. O critério está alinhado ao formato de resposta documentado pela MathWorks.

Não há `created_at` no POST: o horário da entrada é o atribuído pelo servidor na recepção, podendo diferir do instante de amostragem em caso de espera na fila. A cadência usual de 30 s e a pausa mínima da tarefa reduzem a frequência de escrita, mas não garantem aceitação se houver outras fontes escrevendo no mesmo canal, limites de plano ou configuração incorreta. Deve-se verificar a configuração dos campos e o canal correspondente à chave privada.

## Limitações e propostas para avaliação futura

As seguintes propostas não foram aplicadas ao código:

| Limitação observada no desenho | Possível evolução, se exigida |
| --- | --- |
| Tempo/contagem zeram no reset | Persistência com política de gravação e restauração definida; considerar desgaste de flash |
| Energia resulta de potência suposta | Sensor de potência/corrente e validação de medição para obter consumo real |
| Perda de amostras e ausência de reenvio | Buffer limitado com timestamps, política de descarte e tratamento dos limites do serviço |
| Reconexão TLS Blynk pode atrasar o loop | Medir atrasos no ambiente real antes de redesenhar a máquina de conexão |
| Blynk pode tentar antes de NTP confirmado | Considerar uma espera não bloqueante por relógio plausível, com diagnóstico e prazo |
| Versões não registradas | Registrar núcleo, placa/FQBN e versão Blynk da compilação validada |
| Sem medição de recursos | Medir mínimo de heap e margem de pilha durante TLS simultâneo e reconexão |
| Polaridade documental do relé conflitante | Registrar teste local com GPIO=0/1 e LED apagado/aceso no diagrama efetivo |
| Última recuperação não comprovada | Registrar queda e retorno controlados, logs de reconexão, comando V0/V1 e novo `entry_id` |

## Viabilidade de compilação e limites desta revisão

Não foram encontrados no PATH `arduino-cli`, `arduino`, `arduino-builder`, `pio`, `cppcheck`, `clang++` ou `xtensa-esp32-elf-g++`. Também não foram encontradas instalações nos caminhos Arduino/PlatformIO usuais examinados em Program Files, Program Files (x86), AppData Local/Programs, Arduino15, `.arduinoIDE`, Documents/Arduino e `.platformio` do usuário Guiler. A busca foi limitada a esses locais; não varreu todo o disco nem prova inexistência de uma instalação em caminho personalizado.

Não foi executada compilação local nem análise estática com um compilador ESP32. Não foram instaladas toolchains nem criados stubs para simular uma compilação. A revisão cobriu leitura integral, fluxo de dados, temporizações, dependências, conexões JSON e extração dos certificados públicos. A evidência de compilação/execução deve vir do ambiente Wokwi ou de uma toolchain ESP32 real, com suas versões registradas.
