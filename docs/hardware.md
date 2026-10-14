# Hardware do Smart Light

Este documento descreve as conexões do `diagram.json` fornecido e a configuração preservada do firmware. A atividade utiliza um ESP32 simulado, módulo relé, resistor e LED representando uma lâmpada. O ESP32 substitui o ESP8266 do enunciado; a aprovação dessa substituição pelo professor permanece pendente.

O escopo da demonstração é **simulação ou carga de baixa tensão**. O diagrama não é uma instalação residencial de 127/220 V e não deve ser reproduzido como circuito de rede elétrica em protoboard.

## Componentes declarados

| Identificador | Componente no Wokwi | Configuração presente |
| --- | --- | --- |
| `esp` | `board-esp32-devkit-c-v4` | Placa ESP32 DevKitC V4. |
| `relay1` | `wokwi-relay-module` | Atributo `transistor: "npn"`. |
| `r1` | `wokwi-resistor` | Resistência de 220 Ω. |
| `led1` | `wokwi-led` | Cor amarela; rótulo `Lampada`. |

O módulo representa a interface de acionamento do relé. O GPIO envia um sinal de controle ao módulo; a bobina de um relé físico não deve ser alimentada diretamente pelo GPIO.

## Todas as conexões do diagrama

Os identificadores abaixo são transcritos exatamente do arquivo. As duas primeiras linhas pertencem ao monitor serial virtual. Uma conexão elétrica não depende do sentido em que os terminais aparecem na tabela.

| Origem exata | Destino exato | Cor no JSON | Função |
| --- | --- | --- | --- |
| `esp:TX` | `$serialMonitor:RX` | vazia | Diagnóstico enviado pela placa ao monitor. |
| `esp:RX` | `$serialMonitor:TX` | vazia | Recepção serial da placa. |
| `esp:18` | `relay1:IN` | `green` | Comando pelo GPIO18. |
| `esp:5V` | `relay1:VCC` | `red` | Alimentação de 5 V do módulo. |
| `esp:GND.1` | `relay1:GND` | `black` | Referência elétrica do módulo. |
| `esp:5V` | `relay1:COM` | `red` | Alimentação de 5 V a ser comutada. |
| `relay1:NO` | `r1:1` | `green` | Contato normalmente aberto ligado ao resistor. |
| `r1:2` | `led1:A` | `green` | Resistor em série com o ânodo do LED. |
| `led1:C` | `esp:GND.2` | `black` | Retorno do cátodo do LED ao GND. |

O contato `NC` não está conectado. O monitor serial está configurado para permanecer visível; no firmware, `Serial.begin(115200)` define sua velocidade.

## Caminho da corrente e comando

Quando o contato COM–NO fecha, o caminho da carga é:

```text
esp:5V → relay1:COM → relay1:NO → r1:1 → r1:2 → led1:A → led1:C → esp:GND.2
```

COM é o contato comum, NO é normalmente aberto e NC é normalmente fechado. Esses terminais e o sinal IN são definidos na [referência oficial do módulo relé Wokwi](https://docs.wokwi.com/parts/wokwi-relay-module). O contato comuta a alimentação presente em COM; o relé não transforma o sinal de 3,3 V em uma fonte de 5 V.

O firmware fornecido mantém `PIN_RELE = 18`, `RELE_LIGADO = HIGH` e `RELE_DESLIGADO = LOW`, em conjunto com `transistor: "npn"`. O guia relata que essa combinação foi validada no teste anterior do grupo; ela foi preservada nesta documentação. `pinMode(..., OUTPUT)` e `digitalWrite()` configuram e alteram a saída, conforme a [API GPIO do Arduino ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/gpio.html).

**Nota sobre a fonte do simulador:** a página oficial do relé apresenta uma contradição interna: a tabela de atributos descreve NPN como ativo em HIGH, enquanto a seção de operação descreve uma lógica inversa. Assim, a página não deve ser usada isoladamente para inverter o firmware. A documentação registra a combinação NPN/HIGH do projeto e os testes anteriores, sem generalizar essa polaridade para qualquer módulo real.

## GPIO de 3,3 V e módulo de 5 V

O HIGH do GPIO18 pertence ao domínio nominal de 3,3 V do ESP32. O módulo relé e o circuito do LED recebem 5 V pelo pino de alimentação da placa simulada. São pontos com funções diferentes: **GPIO18 controla; 5 V alimenta**.

O nome “módulo de 5 V” não garante que sua entrada IN reconheça corretamente um sinal de 3,3 V. A compatibilidade de uma montagem física está pendente e exige identificar o módulo real, consultar seu limiar de entrada, sua corrente de acionamento e sua alimentação. Também é necessário verificar a fonte e a corrente da carga. O funcionamento no Wokwi não certifica essas condições em uma bancada real.

Não aplicar 5 V diretamente a um GPIO. A [FAQ de hardware da Espressif](https://docs.espressif.com/projects/esp-faq/en/latest/hardware-related/hardware-design.html#what-is-the-voltage-tolerance-of-gpios-of-esp-chips) informa tolerância de 3,6 V, que não deve ser tratada como tensão nominal de operação.

## O resistor de 220 Ω

O resistor está **em série com o LED para limitar a corrente**. Ele não constitui o divisor resistivo clássico de dois resistores. O circuito não implementa conversão de 5 V para 3,3 V por divisor de tensão.

Um exemplo didático pela Lei de Ohm, supondo fonte de 5 V, queda de 2 V no LED e contato ideal, resulta em:

```text
VR = 5 − 2 = 3 V
I = VR / R = (5 − 2) / 220 ≈ 0,0136 A = 13,6 mA
```

**13,6 mA é um cálculo hipotético, não uma medição do protótipo.** A queda real no LED depende do componente e da corrente. O firmware não lê sensor de corrente ou tensão. Da mesma forma, os 9 W empregados no cálculo de energia representam uma lâmpada hipotética, não o consumo desse LED; ver [arquitetura.md](arquitetura.md).

Um divisor ideal usa dois resistores e produz `Vout = Vin × R2 / (R1 + R2)` quando a carga de saída é desprezível. Ele não eleva 3,3 V para 5 V nem substitui um regulador de alimentação. Caso uma futura interface exija adaptação de sinal, seu dimensionamento dependerá das características reais dos dispositivos.

## Alcance das evidências

O Guia do Grupo, com evidências até 11/10/2026, descreve o teste local e o mapeamento NPN/HIGH adotado. Esta documentação registra o circuito e esse histórico; **não inclui uma nova simulação, medição elétrica ou validação de hardware físico**. A confirmação de luminosidade, corrente ou consumo exigiria instrumentação ou sensores apropriados, ausentes no diagrama atual.
