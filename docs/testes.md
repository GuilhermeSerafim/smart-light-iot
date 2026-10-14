# Testes e evidências

As evidências abaixo foram fornecidas pelo grupo em **11/10/2026** e reproduzidas nos materiais originais. A revisão documental de **14/10/2026** não representa uma nova execução do ESP32, do Wokwi, do Blynk ou do ThingSpeak. O estado atual dos serviços não foi consultado nesta revisão.

## Proveniência das capturas

As cinco imagens foram extraídas dos arquivos locais originais, revisadas visualmente e por OCR local. Não foram detectadas credenciais nas imagens; não foi necessário aplicar tarjas. A imagem do circuito aparece nos dois materiais e foi mantida apenas uma vez.

| Captura | Origem exata | O que documenta |
| --- | --- | --- |
| [Circuito Wokwi](../assets/screenshots/circuito-wokwi.png) | `02_Smart_Light_Guia_do_Grupo.pdf`, p. 3; também presente em `03_Smart_Light_Apresentacao (1).html`, slide 3, “02 / CIRCUITO” | Montagem simulada com ESP32, módulo relé, resistor e LED. A imagem não é medição elétrica nem novo ensaio. |
| [Quatro campos do ThingSpeak](../assets/screenshots/thingspeak-quatro-campos.png) | `02_Smart_Light_Guia_do_Grupo.pdf`, p. 11 | Gráficos de estado, tempo, acionamentos e energia estimada; misturam registros antigos e recentes. |
| [Estatísticas do canal](../assets/screenshots/thingspeak-estatisticas.png) | `03_Smart_Light_Apresentacao (1).html`, slide 11, “10 / EVIDÊNCIAS” | 8 registros, criação indicada como “5 days ago” e última entrada como “2 minutes ago” naquele momento. Essas expressões não descrevem a data atual. |
| [Tempo acumulado](../assets/screenshots/thingspeak-tempo.png) | Mesmo HTML, slide 11, “10 / EVIDÊNCIAS” | Recorte do Field 2; uma linha entre datas diferentes não comprova operação contínua. |
| [Blynk Offline](../assets/screenshots/blynk-offline.png) | Mesmo HTML, slide 11, “10 / EVIDÊNCIAS” | Estado Offline no painel final recebido; não mostra recuperação posterior. |

Os PDFs e o HTML originais permanecem como fontes locais. O texto desses três materiais foi lido integralmente; as imagens embutidas também foram inspecionadas. Não foram detectados segredos literais nesses materiais. A auditoria identificou credenciais literais no sketch público de origem; seus valores não são reproduzidos nesta documentação. Remover valores de uma cópia não invalida credenciais já expostas: a rotação deve ser feita nos respectivos serviços.

## Histórico observado e pendências

| Item | Evidência histórica de 11/10/2026 | Limite ou verificação pendente |
| --- | --- | --- |
| Conexão inicial Blynk | `Ready`, `WiFi=ON`, `Blynk=ON`; o guia p. 10 relata sucesso após ampliar a tentativa de 5 s para 30 s. | Não comprova disponibilidade contínua nem recuperação da última queda. |
| Comando e saída | ON → OFF → ON; V0 e GPIO18 alternaram 1 → 0 → 1, com retorno V1 correspondente. | Repetir o controle na versão atual. V1 informa estado lógico, sem sensor para comprovar luz ou continuidade da carga. |
| Contagem | Segundo ON levou os acionamentos de 1 para 2. | Verificar que ON repetido enquanto já ligada não soma outro acionamento. |
| Tempo ligado | Cresceu em ON e permaneceu em `0,004797 h` durante OFF. | Acumulado por execução, sem total diário automático; reinício zera o acumulado. |
| ThingSpeak | `HTTP=200`, `entry_id=8`; captura do canal com 8 registros, anteriormente 3 segundo os materiais. | O identificador 8 não significa oito acionamentos nem oito escritas nesta sessão. Confirmar novas escritas nos quatro campos. |
| Energia | Campo calculado a partir de potência hipotética constante de 9 W e tempo ligado. | Não há medição de corrente/consumo nem prova de economia; não inclui ESP32, relé, fonte e perdas. |
| Última desconexão | Print final Offline e último trecho do log em tentativa de conexão TLS. | Não há resultado posterior; causa não estabelecida. Não atribuir a queda ao envio ThingSpeak sem evidência. |
| Hardware e escopo | Teste em ESP32 simulado no Wokwi, com lógica NPN/HIGH e carga representada por LED. | Confirmar aceite da substituição do ESP8266 previsto no enunciado; hardware físico exige validação própria. |

Fontes textuais: `01_Smart_Light_Resumo.pdf`, pp. 1–3; `02_Smart_Light_Guia_do_Grupo.pdf`, pp. 7, 9–12 e 15; HTML, slides 8–12 e 16–17. A numeração dos slides inclui a capa; os rótulos internos começam depois dela.

### Sequência temporal preservada

O guia p. 11 apresenta estes **trechos selecionados**, com reticências originais indicando campos omitidos. Isto não é um log integral nem uma nova execução:

```text
[V0] recebido=1 | lampada=LIGADA | GPIO18=1 | V1=1
[V0] recebido=0 | lampada=DESLIGADA | GPIO18=0 | V1=0
[STATUS] ... horas=0.004797 | acionamentos=1
[TS] HTTP=200 | entry_id=8 | gravacao confirmada.
[STATUS] ... acionamentos=2
[BLYNK] Tentando conexao TLS...
```

A ordem documentada é primeiro ON, depois OFF, tempo estável e contagem 1, gravação 8, segundo acionamento com contagem 2 e tentativa de reconexão. O segundo acionamento ocorreu **depois** da gravação 8; por isso o Field 3 ainda mostrar 1 naquela captura é compatível com a sequência. É necessária outra escrita para registrar a nova contagem. O material não fornece timestamps completos para reconstruir a duração de todos os eventos.

A imagem final Offline não demonstra o estado físico da lâmpada: um painel pode conservar o último valor. Sem rede, a saída conserva o último estado enquanto o firmware continua rodando; reinício é outro caso, iniciando desligada, zerando contadores e sincronizando V0 após conectar, conforme o comportamento descrito no guia pp. 2, 6 e 14.

## Roteiro de validação a executar

As etapas seguintes são um roteiro proposto, não resultados já obtidos nesta revisão. Preparar credenciais próprias fora de qualquer cópia pública e executar em simulação ou baixa tensão.

| Etapa | `TESTE_LOCAL` | `ATIVAR_THINGSPEAK` | Objetivo |
| --- | --- | --- | --- |
| Local | `true` | `false` | Verificar relé/LED, GPIO18 e mensagens seriais sem depender das nuvens. |
| Blynk | `false` | `false` | Verificar conexão, comando V0 e retorno V1. |
| Integração | `false` | `true` | Verificar controle enquanto os quatro campos são gravados. |

1. No teste local, conferir saída inicial desligada e observar o comportamento automático definido por esse modo no firmware; verificar a polaridade NPN/HIGH e o caminho COM–NO. Não confundir esse teste com um comando remoto.
2. No teste Blynk, aguardar `Ready` e status Online antes de interpretar V0/V1. Enviar ON e confirmar GPIO18=1 e V1=1; enviar OFF e confirmar GPIO18=0 e V1=0.
3. Com a lâmpada já ligada, reenviar ON e comparar a contagem antes/depois. O resultado esperado é nenhuma nova transição OFF→ON. Opcionalmente, por um meio de envio controlado e privado, enviar um valor de V0 diferente de 0/1 para verificar a rejeição; restaurar um comando válido em seguida. Não é necessário alterar permanentemente a configuração do painel.
4. Na integração, sem reiniciar entre ações, manter ON por 60 s de simulação, OFF por cerca de 40 s e ON novamente por cerca de 40 s. Registrar GPIO18, V1, tempo, acionamentos e respostas de escrita em cada fase. Se necessário, aguardar além de cada duração para observar uma escrita positiva após a mudança.
5. Em ON, tempo e energia estimada devem crescer; em OFF, devem parar sem zerar. Apenas o novo OFF→ON deve somar um acionamento. Conferir os campos 1–4 de cada amostra. As durações são de simulação e não devem ser tratadas automaticamente como tempo civil da nuvem.
6. Para cada gravação, exigir **HTTP 200 e corpo com `entry_id > 0`**. HTTP 200 com corpo 0 não confirma a escrita. A criação da tarefa de envio também não prova gravação. Conferir Entries, Last entry e o registro recente, além da curva.
7. Depois da sequência, reiniciar em um teste separado para observar a saída inicial e o reset dos contadores. Verificar também a sincronização de V0; um último comando salvo em ON pode religar a saída após a conexão. Registrar esse comportamento e a separação de sessões.

A cadência programada é de 30 s de simulação. Um pulso rápido pode não aparecer no gráfico de estado. A fila de uma posição substitui a amostra pendente pela mais recente e não oferece histórico local completo; envios com falha podem perder amostras. Não somar os valores acumulados para obter horas por dia, pois isso repete tempo já contado.

### Critério para confirmar recuperação

Provocar uma interrupção controlada e registrar antes, durante e depois, com timestamps e sem revelar credenciais. Confirmar que a conexão volta ao estado conectado, que um **novo** comando altera GPIO18 e V1 corretamente e que uma **nova** amostra dos quatro campos recebe HTTP 200 com identificador positivo. Guardar o log posterior à tentativa de reconexão e as capturas correspondentes. Um `Ready` anterior à queda ou um valor antigo no painel não satisfaz esse critério.

Repetir ON/OFF após recuperar a conexão, com envios ativos, e informar separadamente qualquer falha restante. O envio HTTP em outra tarefa reduz espera no controle, mas a tentativa de reconexão Blynk ainda pode bloquear temporariamente o fluxo principal. Este roteiro verifica o comportamento funcional observado e não estabelece, sozinho, estabilidade prolongada.

## Revisão estática de 14/10/2026

As verificações abaixo foram executadas localmente com Python, Git e revisão documental. Não houve compilação, nova simulação, envio de comandos Blynk ou gravação de telemetria nesta revisão. O código de origem foi obtido do projeto público informado; seus segredos foram substituídos antes de escrever a cópia compartilhável.

| Verificação | Resultado final |
| --- | --- |
| Sintaxe JSON e conexões | `json.loads()` aceitou o arquivo: 4 componentes com IDs únicos e 9 conexões. Referências de componente/pino conferidas. Isso não é validação do esquema completo pelo simulador. |
| Preservação de `diagram.json` e `libraries.txt` | Arquivos idênticos, byte a byte, aos obtidos do Wokwi. |
| Preservação do firmware | Comparação integral, normalizando quebras de linha: somente Auth Token, Write API Key e Template ID foram substituídos por placeholders. GPIO, lógica, constantes, certificados, bibliotecas e comunicação preservados. |
| Dependências e documentação | Dez includes mapeados ao core/plataforma e à biblioteca externa Blynk declarada. As doze funções/callbacks estão no mapa de `firmware.md`. |
| Verificação lexical C++ | Parênteses, chaves e colchetes balanceados após desconsiderar comentários e literais. Não substitui análise sintática, tipagem ou compilação. |
| Certificados públicos | Cinco blocos PEM interpretados com sucesso; os períodos de validade incluem 14/10/2026. Não houve teste da cadeia apresentada pelos servidores. |
| Documentação e imagens | UTF-8 conferido, cercas de código balanceadas e todos os destinos de links locais existentes. Cinco PNGs únicos abrem; OCR local, revisão visual e metadados verificados, sem segredos detectados. Mermaid foi revisado como texto, sem validação por renderizador nesta etapa. |
| Materiais originais | Dois PDFs (3 e 15 páginas) e HTML (18 slides) lidos integralmente, incluindo capturas; preservados na pasta local e excluídos do conjunto publicável pelo `.gitignore`. |
| Varredura de segredos | Sem ocorrências das duas credenciais expostas ou dos padrões sensíveis pesquisados nos 16 arquivos candidatos. Também foram verificados texto extraído, metadados e links dos PDFs e conteúdo do HTML original. Certificados públicos foram diferenciados de chaves privadas. A busca por padrões não é garantia absoluta contra qualquer segredo desconhecido. |
| Regras do `.gitignore` | Verificadas com Git em metadados temporários externos à pasta: `local/`, `.env`, `secrets.h`, artefatos de compilação, captura de rede e os três materiais originais são ignorados. A enumeração de arquivos publicáveis retornou somente os 16 previstos. |
| Histórico Git local | Na inspeção inicial, não havia `.git` na pasta do projeto nem nos diretórios superiores. O versionamento local foi iniciado depois dessa inspeção, em commits separados por responsabilidade. Isso não constitui auditoria de históricos remotos ou de cópias fora da pasta. |
| Compilação / simulação / integração real | Não executadas nesta revisão. Arduino CLI/IDE, PlatformIO e toolchain ESP32 não foram encontrados no PATH e nos locais usuais examinados; `cppcheck` também não estava disponível. O roteiro acima permanece a executar. |

Esta preparação foi organizada em commits locais para permitir rastrear e revisar cada etapa. Nenhum push, repositório remoto ou publicação foi criado. A publicação depende da autorização explícita do responsável e da definição do destino. A licença ainda não foi escolhida; não há `LICENSE`.

### Repetir a checagem de JSON

Na raiz do projeto, com Python disponível, o comando abaixo verifica apenas a sintaxe JSON. Em PowerShell, saída vazia e código de término zero indicam sucesso:

```powershell
python -m json.tool diagram.json > $null
$LASTEXITCODE
```

Antes de publicar, revisar a lista exata de arquivos e o diff preparado para commit. Se o destino já tiver histórico, examiná-lo também. Credenciais que tenham aparecido em commits antigos precisam ser revogadas, mesmo quando ausentes da versão atual; eventual limpeza de histórico é uma ação separada, a planejar sem sobrescrever o trabalho de colegas.
