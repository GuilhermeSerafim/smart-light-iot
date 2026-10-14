// Smart Light — ESP32 + rele NPN no Wokwi + Blynk + ThingSpeak.
// Use somente simulacao/carga de baixa tensao neste circuito.
// ESP32 substitui o ESP8266 do enunciado: confirme com o professor.

#define BLYNK_TEMPLATE_ID "COLE_SEU_BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Smart Light"
#define BLYNK_AUTH_TOKEN "COLE_SEU_BLYNK_AUTH_TOKEN"
#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32_SSL.h>
#include <HTTPClient.h>
#include <time.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

constexpr bool TESTE_LOCAL = false;
constexpr bool ATIVAR_THINGSPEAK = true;

const char* THINGSPEAK_WRITE_API_KEY = "COLE_SUA_THINGSPEAK_WRITE_API_KEY";
constexpr uint8_t PIN_RELE = 18;
// Estes niveis correspondem a transistor="npn" no diagram.json fornecido.
constexpr uint8_t RELE_LIGADO = HIGH;
constexpr uint8_t RELE_DESLIGADO = LOW;
constexpr double POTENCIA_LAMPADA_W = 9.0; // Hipotese, nao medicao.
constexpr uint32_t INTERVALO_ENVIO_MS = 30000;
constexpr uint32_t INTERVALO_STATUS_MS = 5000;

bool lampadaLigada = false;
unsigned long numeroAcionamentos = 0;
uint64_t tempoLigadaMs = 0;
uint32_t ultimaMedicaoMs = 0;
uint32_t ultimoTesteMs = 0;
uint32_t ultimoStatusMs = 0;
uint32_t ultimoEnvioMs = 0;
uint32_t ultimaTentativaBlynkMs = 0;
uint32_t ultimaTentativaWiFiMs = 0;
bool iniciouNtp = false;
bool podeConectarBlynk = false;

// Apenas a tarefa principal altera estado, GPIO e Blynk.
// A fila copia os dados para a tarefa HTTP: nao compartilha variaveis mutaveis.
struct Amostra {
  int estado;
  double horas;
  unsigned long acionamentos;
  double energiaKWh;
};
QueueHandle_t filaThingSpeak = nullptr;

// Certificados PUBLICOS de autoridades confiaveis, definidos ao final.
// Nao sao credenciais. Nao usamos setInsecure().
extern const char THINGSPEAK_CA[];

void atualizarTempo() {
  const uint32_t agora = millis();
  const uint32_t decorrido = agora - ultimaMedicaoMs;
  if (lampadaLigada) {
    tempoLigadaMs += static_cast<uint64_t>(decorrido);
  }
  ultimaMedicaoMs = agora;
}

void aplicarEstado(bool novoEstado) {
  atualizarTempo(); // Contabiliza o intervalo ANTES de trocar o estado.
  if (novoEstado && !lampadaLigada) {
    numeroAcionamentos++;
  }
  lampadaLigada = novoEstado;
  digitalWrite(PIN_RELE, lampadaLigada ? RELE_LIGADO : RELE_DESLIGADO);
}

void enviarFeedback() {
  if (Blynk.connected()) {
    // Estado LOGICO aplicado. Nao confirma luminosidade/consumo fisico.
    Blynk.virtualWrite(V1, lampadaLigada ? 1 : 0);
  }
}

BLYNK_WRITE(V0) {
  const int comando = param.asInt();
  if (comando != 0 && comando != 1) {
    Serial.printf("[V0] Comando invalido: %d (esperado 0 ou 1).\n", comando);
    return;
  }
  aplicarEstado(comando == 1);
  enviarFeedback();
  Serial.printf("[V0] recebido=%d | lampada=%s | GPIO18=%d | V1=%d\n",
                comando, lampadaLigada ? "LIGADA" : "DESLIGADA",
                digitalRead(PIN_RELE), lampadaLigada ? 1 : 0);
}

BLYNK_CONNECTED() {
  Serial.println("[BLYNK] Online. Solicitando ultimo valor de V0...");
  // A resposta chega pelo BLYNK_WRITE(V0), que aplica a saida e envia V1.
  Blynk.syncVirtual(V0);
}

void imprimirStatus() {
  Serial.printf("[STATUS] modo=%s | WiFi=%s | Blynk=%s | logica=%d | GPIO18=%d | horas=%.6f | acionamentos=%lu\n",
                TESTE_LOCAL ? "LOCAL" : "NUVEM",
                WiFi.status() == WL_CONNECTED ? "ON" : "OFF",
                Blynk.connected() ? "ON" : "OFF",
                lampadaLigada ? 1 : 0, digitalRead(PIN_RELE),
                static_cast<double>(tempoLigadaMs) / 3600000.0,
                numeroAcionamentos);
}

void manterConexoes() {
  if (WiFi.status() != WL_CONNECTED) {
    if (Blynk.connected()) {
      Blynk.disconnect();
    }
    if (static_cast<uint32_t>(millis() - ultimaTentativaWiFiMs) >= 15000) {
      Serial.println("[WIFI] Tentando reconectar ao Wokwi-GUEST...");
      WiFi.reconnect();
      ultimaTentativaWiFiMs = millis();
    }
    return;
  }

  if (!iniciouNtp) {
    // Usado para validar a vigencia dos certificados TLS.
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    iniciouNtp = true;
    Serial.print("[WIFI] Conectado. IP: ");
    Serial.println(WiFi.localIP());
  }

  if (Blynk.connected()) {
    Blynk.run();
  } else if (static_cast<uint32_t>(millis() - ultimaTentativaBlynkMs) >= 10000) {
    Serial.println("[BLYNK] Tentando conexao TLS...");
    // O parametro limita a espera do protocolo Blynk; DNS/TLS podem levar mais.
    const bool conectado = Blynk.connect(30000);
    ultimaTentativaBlynkMs = millis();
    if (!conectado) {
      Serial.println("[BLYNK] Sem conexao. Confira token e mensagens TLS acima.");
    }
  }
}

// Executa na tarefa HTTP, nunca dentro de BLYNK_WRITE ou do loop principal.
void enviarThingSpeak(int estado, double horas,
                      unsigned long acionamentos, double energiaKWh) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TS] Sem WiFi; amostra nao enviada.");
    return;
  }
  if (time(nullptr) < 1704067200) { // 01/01/2024: ainda sem relogio valido.
    Serial.println("[TS] Aguardando horario NTP para validar TLS.");
    return;
  }

  WiFiClientSecure clienteTLS;
  clienteTLS.setCACert(THINGSPEAK_CA);
  clienteTLS.setHandshakeTimeout(8); // segundos, nao milissegundos

  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(5000);

  // URL simples: nao colocar [texto](link) dentro desta string.
  if (!http.begin(clienteTLS, "https://api.thingspeak.com/update")) {
    Serial.println("[TS] Falha ao preparar requisicao HTTPS.");
    return;
  }

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Chave no cabecalho, nao na URL nem nos logs.
  http.addHeader("THINGSPEAKAPIKEY", THINGSPEAK_WRITE_API_KEY);
  String corpo = "field1=" + String(estado)
               + "&field2=" + String(horas, 6)
               + "&field3=" + String(acionamentos)
               + "&field4=" + String(energiaKWh, 8);

  Serial.println("[TS] Iniciando envio HTTPS em tarefa separada...");
  const int codigo = http.POST(corpo);
  String resposta = codigo > 0 ? http.getString() : String();
  resposta.trim();

  char* fim = nullptr;
  const long entrada = strtol(resposta.c_str(), &fim, 10);
  const bool salvo = codigo == HTTP_CODE_OK && entrada > 0 && fim && *fim == '\0';
  if (salvo) {
    Serial.printf("[TS] HTTP=200 | entry_id=%ld | gravacao confirmada.\n", entrada);
  } else if (codigo < 0) {
    Serial.printf("[TS] Erro HTTP/TLS=%d: %s\n", codigo,
                  HTTPClient::errorToString(codigo).c_str());
    char erroTLS[160] = {};
    const int erro = clienteTLS.lastError(erroTLS, sizeof(erroTLS));
    if (erro != 0) {
      Serial.printf("[TS] TLS=%d: %s\n", erro, erroTLS);
    }
  } else {
    Serial.printf("[TS] HTTP=%d | gravacao NAO confirmada. Confira chave/canal/intervalo.\n", codigo);
    if (resposta == "0") {
      Serial.println("[TS] Corpo=0: o ThingSpeak recusou a gravacao.");
    }
  }
  http.end();
  clienteTLS.stop();
}

void tarefaThingSpeak(void* argumento) {
  (void)argumento;
  Amostra amostra;
  for (;;) {
    if (xQueueReceive(filaThingSpeak, &amostra, portMAX_DELAY) == pdTRUE) {
      enviarThingSpeak(amostra.estado, amostra.horas,
                       amostra.acionamentos, amostra.energiaKWh);
      // Este descanso pausa SOMENTE esta tarefa, nao o processamento do Blynk.
      // Evita disparos proximos mesmo se houver amostra aguardando na fila.
      vTaskDelay(pdMS_TO_TICKS(20000));
    }
  }
}

void prepararThingSpeak() {
  if (!ATIVAR_THINGSPEAK) {
    Serial.println("[TS] Desativado para testar primeiro o Blynk.");
    return;
  }
  if (String(THINGSPEAK_WRITE_API_KEY).startsWith("COLE_")) {
    Serial.println("[TS] Preencha sua nova Write API Key. Envios desativados.");
    return;
  }
  filaThingSpeak = xQueueCreate(1, sizeof(Amostra));
  if (!filaThingSpeak) {
    Serial.println("[TS] Sem memoria para criar fila. Blynk continua funcionando.");
    return;
  }
  const BaseType_t resultado = xTaskCreate(
    tarefaThingSpeak, "thingspeak", 12288, nullptr, 1, nullptr);
  if (resultado != pdPASS) {
    vQueueDelete(filaThingSpeak);
    filaThingSpeak = nullptr;
    Serial.println("[TS] Falha ao criar tarefa. Blynk continua funcionando.");
  } else {
    Serial.println("[TS] Tarefa criada. Amostras programadas a cada 30 s.");
  }
}

void setup() {
  Serial.begin(115200);
  // Estado inicial desligado para o rele NPN deste diagrama.
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, RELE_DESLIGADO);
  ultimaMedicaoMs = millis();
  ultimoTesteMs = millis();

  Serial.println("\n[BOOT] Smart Light - diagrama NPN + GPIO18 HIGH para ligar.");
  if (TESTE_LOCAL) {
    Serial.println("[MODO] TESTE LOCAL: alterna a lampada a cada 2 s; nuvens nao conectadas.");
    Serial.println("[LOCAL] GPIO18=0 -> lampada deve estar APAGADA.");
    return;
  }

  if (String(BLYNK_AUTH_TOKEN).startsWith("COLE_")) {
    Serial.println("[CONFIG] Preencha o NOVO BLYNK_AUTH_TOKEN antes de testar a nuvem.");
    return;
  }

  podeConectarBlynk = true;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin("Wokwi-GUEST", "", 6);
  Blynk.config(BLYNK_AUTH_TOKEN);
  ultimaTentativaBlynkMs = millis() - 10000;
  ultimaTentativaWiFiMs = millis();
  prepararThingSpeak();
  Serial.println("[MODO] NUVEM: aguardando WiFi e Blynk...");
}

void loop() {
  atualizarTempo();

  if (TESTE_LOCAL) {
    if (static_cast<uint32_t>(millis() - ultimoTesteMs) >= 2000) {
      ultimoTesteMs = millis();
      aplicarEstado(!lampadaLigada);
      Serial.printf("[LOCAL] GPIO18=%d -> lampada deve estar %s.\n",
                    digitalRead(PIN_RELE), lampadaLigada ? "ACESA" : "APAGADA");
    }
  } else if (podeConectarBlynk) {
    manterConexoes();
    atualizarTempo(); // Inclui tempo transcorrido numa tentativa de reconexao.

    if (filaThingSpeak && WiFi.status() == WL_CONNECTED &&
        static_cast<uint32_t>(millis() - ultimoEnvioMs) >= INTERVALO_ENVIO_MS) {
      ultimoEnvioMs = millis();
      const double horas = static_cast<double>(tempoLigadaMs) / 3600000.0;
      Amostra amostra = {
        lampadaLigada ? 1 : 0, horas, numeroAcionamentos,
        (POTENCIA_LAMPADA_W / 1000.0) * horas
      };
      // Guarda somente a amostra mais recente; nao ha backlog de reenvios.
      xQueueOverwrite(filaThingSpeak, &amostra);
    }
  }

  if (static_cast<uint32_t>(millis() - ultimoStatusMs) >= INTERVALO_STATUS_MS) {
    ultimoStatusMs = millis();
    imprimirStatus();
  }
  // Cede CPU ao sistema. Nao e uma espera longa de controle/telemetria.
  delay(1);
}

// Certificados raiz PUBLICOS. Validacao TLS permanece habilitada.
// A primeira CA corresponde ao exemplo oficial ESP32/ReadMultipleFieldsSecure
// da MathWorks. As demais sao raizes publicas do repositório de confianca
// do sistema, para cadeias alternativas de certificados dos servicos HTTPS.
// Atualize estas raizes se uma futura cadeia do servidor exigir outra CA.
const char THINGSPEAK_CA[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL
MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug
RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm
+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW
PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM
xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB
Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3
hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg
EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF
MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA
FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec
nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z
eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF
hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2
Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe
vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
+OkuE6N36B9K
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)PEM";
