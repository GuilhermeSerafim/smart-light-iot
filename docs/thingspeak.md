# ThingSpeak: armazenamento e interpretação das métricas

[Voltar ao README](../README.md)

O ThingSpeak recebe amostras calculadas pelo ESP32 e permite visualizar seu histórico. O firmware usa `HTTPClient` e `WiFiClientSecure` do Arduino-ESP32; não inclui nem precisa da biblioteca Arduino `ThingSpeak` nesta implementação.

## Configurar o canal

1. Criar um canal Smart Light no ThingSpeak e habilitar os campos 1 a 4.
2. Nomear os campos conforme a tabela. Indicar as unidades nos títulos/eixos dos gráficos.
3. Em API Keys, obter a **Write API Key**, mantendo-a privada. A Read API Key tem outra finalidade e não substitui a chave de escrita.
4. Preencher `THINGSPEAK_WRITE_API_KEY` somente na cópia privada de execução.
5. Manter `ATIVAR_THINGSPEAK = true` e `TESTE_LOCAL = false` para a integração. O token Blynk também precisa estar configurado: seu placeholder faz `setup()` retornar antes de inicializar Wi-Fi e a tarefa HTTP.

| Campo | Nome sugerido | Origem no firmware | Unidade / interpretação |
| --- | --- | --- | --- |
| Field 1 | Estado lógico da lâmpada | `lampadaLigada ? 1 : 0` | 0=desligada, 1=ligada |
| Field 2 | Tempo acumulado ligada | `tempoLigadaMs / 3600000.0` | horas nesta execução |
| Field 3 | Número de acionamentos | `numeroAcionamentos` | transições OFF → ON nesta execução |
| Field 4 | Energia estimada | `(POTENCIA_LAMPADA_W / 1000.0) * horas` | kWh nesta execução, supondo 9 W |

O Channel ID identifica o canal para navegação e consultas. A escrita atual seleciona o canal por sua Write API Key; não existe variável de Channel ID a preencher no sketch. [Propriedades do canal](https://www.mathworks.com/help/thingspeak/channel-settings.html).

## Requisição e confirmação

`enviarThingSpeak()` faz POST para `https://api.thingspeak.com/update`, usando o cabeçalho `THINGSPEAKAPIKEY`. A credencial não é colocada na URL nem nos logs. O corpo tem formato `application/x-www-form-urlencoded` e inclui os quatro campos. Horas são formatadas com seis casas decimais e energia com oito.

Exemplo didático de corpo, sem credenciais e sem representar um teste:

```text
field1=1&field2=3.000000&field3=2&field4=0.02700000
```

O firmware só imprime **gravação confirmada** quando há HTTP 200 e o corpo, após remoção de espaços nas extremidades, representa um inteiro positivo completo. HTTP 200 com corpo `0` é recusa de gravação. Uma mensagem de tarefa criada ou de início de envio não comprova armazenamento. [API oficial de escrita](https://www.mathworks.com/help/thingspeak/writedata.html).

## Tempo, fila e falhas

O loop programa uma amostra a cada 30 s de simulação, se houver Wi-Fi e fila criada. A tarefa HTTP recebe uma cópia de `Amostra` por uma fila de uma posição, faz a tentativa de envio e espera 20 s antes de receber outra. O período efetivo também depende do loop, das conexões e do tempo da requisição; não há garantia de uma gravação a cada 30 s.

`xQueueOverwrite()` conserva apenas a amostra pendente mais recente. Uma falha de envio não gera reenvio persistente. Sem Wi-Fi, o loop não enfileira novas amostras. Se somente o Blynk estiver offline, o envio ao ThingSpeak ainda pode ocorrer, sujeito às esperas de reconexão na tarefa principal.

A API aplica limites de frequência conforme a licença; a modalidade gratuita documenta intervalo mínimo de 15 s. Os 30 s programados e a espera da tarefa foram preservados, sem reduzir intervalos. O tempo do simulador e o relógio de recebimento da nuvem não são intercambiáveis. [Limites de licenciamento ThingSpeak](https://thingspeak.mathworks.com/files/LicenseSpecsAndFAQ.pdf).

O cliente HTTPS valida certificados com o conjunto público `THINGSPEAK_CA`. Antes do envio, exige `time(nullptr) >= 1704067200`, uma verificação mínima de que o relógio deixou o estado inicial. Isso não prova que a hora está exata. NTP, DNS, internet, cadeia de certificados e chave válida continuam necessários; `setInsecure()` não é usado.

## Energia e total diário

Energia estimada é potência hipotética multiplicada pelo tempo lógico ligado:

```text
horas = tempoLigadaMs / 3.600.000
energia_kWh = (9 / 1.000) × horas
```

Três horas corresponderiam a 0,027 kWh. Trata-se de exemplo calculado, não de consumo medido. Não são incluídos ESP32, relé, fonte ou perdas. O projeto não tem sensor de corrente nem de luminosidade.

Todos os acumulados reiniciam com o ESP32. Não somar sucessivos valores de Field 2 ou Field 4: isso contaria repetidamente os mesmos intervalos. Diferenças dentro de uma sessão podem representar incrementos; reinícios, lacunas, limites de dia e fuso exigem tratamento adicional. O total automático por dia ainda não foi implementado.

## Ler o resultado sem extrapolar

Conferir a resposta do firmware, o último registro do canal e os quatro valores recebidos. Ajustar o período de visualização para o ensaio atual. Um gráfico que conecta registros separados por dias não comprova operação contínua. Pulsos ON/OFF entre amostras podem não aparecer em Field 1, embora os acumulados possam registrar seu efeito.

O material de 11/10/2026 registra `HTTP=200` e `entry_id=8`. Esse número é o identificador da entrada, não oito acionamentos nem oito registros produzidos nessa sessão. O segundo acionamento veio depois dessa escrita; o gráfico ainda mostrar 1 era compatível com a sequência. Consulte [evidências e testes](testes.md).

Na inspeção de 14/10/2026, uma Write API Key literal estava no código público de origem. Regenerar essa chave no canal e invalidar a anterior. Manter o código compartilhável com placeholder e revisar cópias antigas; não basta removê-la do próximo commit.
