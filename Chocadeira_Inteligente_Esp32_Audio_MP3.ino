// Parte 1 - Bibliotecas, definições e variáveis globais. ######################################################################################################################################################################################################

// === Parte 1: Bibliotecas e definições ===
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <SD.h>
#include <SPI.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <DFRobotDFPlayerMini.h>
#include <time.h>

// 🎙️ DFPlayer via UART1
HardwareSerial dfSerial(1); // ESP32 UART1
DFRobotDFPlayerMini dfPlayer;

// 📟 Displays
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// 🌐 Wi-Fi e Telegram
const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";
const char* botToken = "SEU_BOT_TOKEN";
const String chat_id = "SEU_CHAT_ID";
WiFiClientSecure secureClient;
UniversalTelegramBot bot(botToken, secureClient);

// 🌐 Web Server
AsyncWebServer server(80);

// 🔧 Pinos
#define RELE_MOTOR 32
#define BUZZER 33
#define PIN_RESISTENCIA 25
#define PIN_UMIDIFICADOR 26
#define PIN_COOLER 27
#define PIN_NIVEL_AGUA 34
#define PIN_SD_CS 5
#define BTN_UP 12
#define BTN_DOWN 13
#define BTN_SELECT 14
#define BTN_BACK 15

// 🧠 EEPROM
#define EEPROM_SIZE 64

// 🐣 Variáveis globais
float tempAtual = 0.0;
float umidAtual = 0.0;
int nivelAgua = 0;
int diaAtual = 1;
int totalDias = 21;
int ovosIncubados = 0;
int pintinhosNascidos = 0;
int especieSelecionada = 0;
bool resistenciaLigada = false;
bool umidificadorLigado = false;
bool ovoscopiaAvisoEnviado = false;
int ultimaViragemHora = -1;
int tempoViragemSegundos = 5;
int intervaloViragemHoras = 3;
int estadoSistema = 1; // 1 = ativo, 0 = finalizado

// 🐥 Espécies disponíveis
String especies[5] = {"Galinha", "Codorna", "Pato", "Peru", "Galinha de Angola"};

// 🎙️ Protótipos das funções
void tocarAudio(int faixa);
void salvarEstado();

// Parte 2 - setup() Completo. #############################################################################################################################################################################################################################

void setup() {
  Serial.begin(115200);

  // 🖥️ Inicializa displays
  lcd.init();
  lcd.backlight();
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  // 🔈 Inicializa DFPlayer Mini
  dfSerial.begin(9600, SERIAL_8N1, 17, 16); // RX=17, TX=16
  if (dfPlayer.begin(dfSerial)) {
    dfPlayer.volume(20);
    Serial.println("DFPlayer OK");
    tocarAudio(25); // Saudação com autoria
  } else {
    Serial.println("Erro DFPlayer");
  }

  // 📡 Conecta Wi-Fi
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi conectado!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  Serial.println(WiFi.localIP());

  // 🕒 Configura NTP
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // 📲 Telegram
  secureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  bot.sendMessage(chat_id, "🐣 Chocadeira conectada!\nIP: " + WiFi.localIP().toString());
  beepConfirmacao(2);

  // 💾 Inicializa cartão SD
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("Erro SD");
    lcd.setCursor(0, 1);
    lcd.print("SD NAO INICIADO");
  } else {
    Serial.println("SD OK");
  }

  // 🧠 EEPROM
  EEPROM.begin(EEPROM_SIZE);
  tempoViragemSegundos = EEPROM.read(0);
  intervaloViragemHoras = EEPROM.read(1);
  especieSelecionada = EEPROM.read(2);
  ovosIncubados = EEPROM.read(3);
  pintinhosNascidos = EEPROM.read(4);

  // 🌐 Inicializa Web Server
  setupWeb();

  // 🧠 Estado inicial
  estadoSistema = 1;
  ultimaViragemHora = -1;
}

// Parte 3 - loop() principal + sensores + viragem + eventos + áudio. #####################################################################################################################################################################################

void salvarEstado() {
  EEPROM.write(0, tempoViragemSegundos);
  EEPROM.write(1, intervaloViragemHoras);
  EEPROM.write(2, especieSelecionada);
  EEPROM.write(3, ovosIncubados);
  EEPROM.write(4, pintinhosNascidos);
  EEPROM.commit();
}

void tocarAudio(int faixa) {
  dfPlayer.play(faixa);
}

void beepConfirmacao(int tipo) {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int h = timeinfo.tm_hour;
  if (h >= 22 || h < 6) return;

  if (tipo == 1) {
    digitalWrite(BUZZER, HIGH); delay(200); digitalWrite(BUZZER, LOW);
  } else {
    for (int i = 0; i < 2; i++) {
      digitalWrite(BUZZER, HIGH); delay(150); digitalWrite(BUZZER, LOW); delay(150);
    }
  }
}

void registrarEventoCSV(String tipo, String detalhes) {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  String data = String(timeinfo.tm_mday) + "/" + String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_year + 1900);
  String hora = String(timeinfo.tm_hour) + ":" + (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);

  File arquivo = SD.open("/ciclo_final.csv", FILE_APPEND);
  if (arquivo) {
    arquivo.println(tipo + "," + data + "," + hora + "," +
                    String(tempAtual) + "," + String(umidAtual) + "," + detalhes);
    arquivo.close();
  }
}

void gerarRelatorioCSV() {
  if (SD.exists("/ciclo_final.csv")) {
    bot.sendMessage(chat_id, "📄 O relatório está salvo no cartão SD como 'ciclo_final.csv'.");
    tocarAudio(22);
  } else {
    bot.sendMessage(chat_id, "⚠️ Nenhum log encontrado no cartão SD.");
  }
}

void controlarClima() {
  if (!resistenciaLigada && tempAtual < 36.5) {
    digitalWrite(PIN_RESISTENCIA, LOW);
    resistenciaLigada = true;
    registrarEventoCSV("Resistência ligada", "Temp: " + String(tempAtual));
    tocarAudio(11);
  } else if (resistenciaLigada && tempAtual > 38.0) {
    digitalWrite(PIN_RESISTENCIA, HIGH);
    resistenciaLigada = false;
    registrarEventoCSV("Resistência desligada", "Temp: " + String(tempAtual));
  }

  if (!umidificadorLigado && umidAtual < 55.0) {
    digitalWrite(PIN_UMIDIFICADOR, LOW);
    umidificadorLigado = true;
    registrarEventoCSV("Umidificador ligado", "Umid: " + String(umidAtual));
    tocarAudio(12);
  } else if (umidificadorLigado && umidAtual > 75.0) {
    digitalWrite(PIN_UMIDIFICADOR, HIGH);
    umidificadorLigado = false;
    registrarEventoCSV("Umidificador desligado", "Umid: " + String(umidAtual));
  }

  digitalWrite(PIN_COOLER, LOW);
  tocarAudio(13);
}

// Parte 4 - Comandos Telegram + Menus LCD + Áudio. ###########################################################################################################################################################################################################

void controlarViragem() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int h = timeinfo.tm_hour;

  if ((diaAtual >= 2) && (diaAtual <= totalDias - 3)) {
    if (ultimaViragemHora == -1 || ((h - ultimaViragemHora + 24) % 24 >= intervaloViragemHoras)) {
      digitalWrite(RELE_MOTOR, LOW);
      delay(tempoViragemSegundos * 1000);
      digitalWrite(RELE_MOTOR, HIGH);
      ultimaViragemHora = h;
      salvarEstado();
      registrarEventoCSV("Viragem automática", "-");
      tocarAudio(4);
      tocarAudio(14);
    }
  }
}

void verificarOvoscopia() {
  if (diaAtual == 7 && !ovoscopiaAvisoEnviado) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("⚠️ Fazer ovoscopia!");
    bot.sendMessage(chat_id, "⚠️ Hoje é o 7º dia. Realize a ovoscopia.");
    beepConfirmacao(2);
    tocarAudio(2);
    salvarEstado();
    ovoscopiaAvisoEnviado = true;
    registrarEventoCSV("Aviso ovoscopia", "Telegram + LCD");
  }
  if (diaAtual > 7) {
    ovoscopiaAvisoEnviado = false;
  }
}

void finalizarCiclo(String origem) {
  estadoSistema = 0;
  digitalWrite(RELE_MOTOR, HIGH);
  digitalWrite(PIN_RESISTENCIA, HIGH);
  digitalWrite(PIN_UMIDIFICADOR, HIGH);
  digitalWrite(PIN_COOLER, HIGH);
  registrarEventoCSV("Finalização manual", origem);
  gerarRelatorioCSV();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ciclo finalizado!");
  beepConfirmacao(2);
  tocarAudio(9);
}

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    diaAtual = (timeinfo.tm_yday % totalDias) + 1;
    tempAtual = 37.5 + (random(-10, 10) / 10.0); // simulado
    umidAtual = 65.0 + (random(-10, 10) / 10.0); // simulado
  }

  nivelAgua = analogRead(PIN_NIVEL_AGUA);
  if (nivelAgua < 2500) {
    lcd.setCursor(0, 1);
    lcd.print("⚠️ Agua BAIXA!");
    bot.sendMessage(chat_id, "⚠️ Nível de água baixo!");
    registrarEventoCSV("Alerta água baixa", String(nivelAgua));
    beepConfirmacao(2);
    tocarAudio(3);
    tocarAudio(18);
  }

  if (estadoSistema == 1) {
    controlarViragem();
    controlarClima();
    verificarOvoscopia();
  }

  processarTelegram(); // Parte 5

  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print("Temp: ");
  oled.print(tempAtual);
  oled.print("C");
  oled.setCursor(0, 10);
  oled.print("Umid: ");
  oled.print(umidAtual);
  oled.print("%");
  oled.setCursor(0, 20);
  oled.print("Agua: ");
  oled.print(nivelAgua);
  oled.display();

  delay(5000);
}

// 5 Parte - Comandos via Telegram. ######################################################################################################################################################################################################################################

void processarTelegram() {
  int novas = bot.getUpdates(bot.last_message_received + 1);
  for (int i = 0; i < novas; i++) {
    String text = bot.messages[i].text;

    if (text == "/status") {
      String s = "🐣 Status:\nEspécie: " + especies[especieSelecionada] +
                 "\nDia: " + String(diaAtual) + "/" + String(totalDias) +
                 "\nTemp: " + String(tempAtual) + "°C\nUmid: " + String(umidAtual) + "%" +
                 "\nOvos: " + String(ovosIncubados) +
                 "\nPintinhos: " + String(pintinhosNascidos);
      bot.sendMessage(chat_id, s);
      beepConfirmacao(2);
      tocarAudio(7);
    }

    else if (text == "/finalizar") {
      finalizarCiclo("Telegram");
    }

    else if (text == "/cancelar") {
      estadoSistema = 0;
      digitalWrite(RELE_MOTOR, HIGH);
      digitalWrite(PIN_RESISTENCIA, HIGH);
      digitalWrite(PIN_UMIDIFICADOR, HIGH);
      digitalWrite(PIN_COOLER, HIGH);
      registrarEventoCSV("Cancelamento manual", "Telegram");
      bot.sendMessage(chat_id, "⚠️ Ciclo cancelado manualmente.");
      beepConfirmacao(2);
      tocarAudio(5);
      tocarAudio(10);
    }

    else if (text.startsWith("/config_viragem ")) {
      int s = text.substring(16, text.indexOf(" ", 16)).toInt();
      int h = text.substring(text.indexOf(" ", 16) + 1).toInt();
      tempoViragemSegundos = s;
      intervaloViragemHoras = h;
      salvarEstado();
      bot.sendMessage(chat_id, "🔧 Viragem ajustada: " + String(s) + "s a cada " + String(h) + "h");
      registrarEventoCSV("Configuração viragem", "Telegram");
      beepConfirmacao(2);
      tocarAudio(21);
    }

    else if (text == "/virar") {
      digitalWrite(RELE_MOTOR, LOW);
      delay(tempoViragemSegundos * 1000);
      digitalWrite(RELE_MOTOR, HIGH);
      registrarEventoCSV("Viragem manual", "Telegram");
      bot.sendMessage(chat_id, "🔄 Viragem manual realizada.");
      beepConfirmacao(2);
      tocarAudio(4);
      tocarAudio(15);
    }

    else if (text.startsWith("/registrar_ovos ")) {
      ovosIncubados = text.substring(17).toInt();
      salvarEstado();
      bot.sendMessage(chat_id, "🥚 Ovos registrados: " + String(ovosIncubados));
      registrarEventoCSV("Registro de ovos", "Qtd: " + String(ovosIncubados));
      beepConfirmacao(2);
      tocarAudio(23);
    }

    else if (text.startsWith("/registrar_pintinhos ")) {
      pintinhosNascidos = text.substring(22).toInt();
      salvarEstado();
      bot.sendMessage(chat_id, "🐥 Pintinhos registrados: " + String(pintinhosNascidos));
      registrarEventoCSV("Registro de pintinhos", "Qtd: " + String(pintinhosNascidos));
      beepConfirmacao(2);
      tocarAudio(24);
    }

    else if (text.startsWith("/especie ")) {
      String arg = text.substring(9);
      bool reconhecido = false;
      for (int j = 0; j < 5; j++) {
        if (arg.equalsIgnoreCase(especies[j]) || arg == String(j)) {
          especieSelecionada = j;
          reconhecido = true;
          break;
        }
      }
      if (reconhecido) {
        salvarEstado();
        registrarEventoCSV("Configuração espécie", especies[especieSelecionada] + " (Telegram)");
        bot.sendMessage(chat_id, "✅ Espécie selecionada: " + especies[especieSelecionada]);
        beepConfirmacao(2);
        tocarAudio(21);
      } else {
        bot.sendMessage(chat_id, "❌ Espécie inválida.");
      }
    }
  }
}

// 📟 Menu físico: configurar viragem
void menuConfigurarViragem() {
  int tempoSelecionado = tempoViragemSegundos;
  int intervaloSelecionado = intervaloViragemHoras;
  int etapa = 0;

  while (true) {
    lcd.clear();
    if (etapa == 0) {
      lcd.setCursor(0, 0);
      lcd.print("Tempo viragem:");
      lcd.setCursor(0, 1);
      lcd.print(String(tempoSelecionado) + " seg");
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Intervalo:");
      lcd.setCursor(0, 1);
      lcd.print(String(intervaloSelecionado) + " horas");
    }

    if (digitalRead(BTN_UP) == LOW) {
      if (etapa == 0) tempoSelecionado++;
      else intervaloSelecionado++;
      delay(300);
    }

    if (digitalRead(BTN_DOWN) == LOW) {
      if (etapa == 0 && tempoSelecionado > 1) tempoSelecionado--;
      else if (etapa == 1 && intervaloSelecionado > 1) intervaloSelecionado--;
      delay(300);
    }

    if (digitalRead(BTN_SELECT) == LOW) {
      if (etapa == 0) etapa = 1;
      else break;
      delay(300);
    }

    if (digitalRead(BTN_BACK) == LOW) break;
    delay(100);
  }

  tempoViragemSegundos = tempoSelecionado;
  intervaloViragemHoras = intervaloSelecionado;
  salvarEstado();
  registrarEventoCSV("Configuração viragem", "Botão LCD");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Viragem ajustada!");
  beepConfirmacao(2);
  tocarAudio(21);
  delay(1000);
}

// 📟 Menu físico: cancelar ciclo
void menuCancelarCiclo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cancelar ciclo?");
  lcd.setCursor(0, 1);
  lcd.print("Pressione SELECT");

  unsigned long espera = millis();
  while (millis() - espera < 8000) {
    if (digitalRead(BTN_SELECT) == LOW) {
      estadoSistema = 0;
      digitalWrite(RELE_MOTOR, HIGH);
      digitalWrite(PIN_RESISTENCIA, HIGH);
      digitalWrite(PIN_UMIDIFICADOR, HIGH);
      digitalWrite(PIN_COOLER, HIGH);
      registrarEventoCSV("Cancelamento manual", "Botão LCD");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ciclo cancelado");
      beepConfirmacao(2);
      tocarAudio(5);
      tocarAudio(10);
      delay(1500);
      return;
    }

    if (digitalRead(BTN_BACK) == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cancelamento abortado");
      delay(1000);
      return;
    }
    delay(100);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tempo expirado");
  delay(1000);
}

// Parte 6 - Interface Web + Endpoint /json. #############################################################################################################################################################################################################################

void setupWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
      <!DOCTYPE html><html><head>
      <title>Chocadeira Inteligente</title>
      <meta charset="UTF-8">
      <style>
        body { font-family: Arial; margin: 20px; background: #f7f7f7; }
        h2 { color: #2c3e50; }
        canvas { margin: 20px 0; }
        span { font-weight: bold; }
      </style>
      <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
      </head><body>
      <h2>Status da Chocadeira</h2>
      <p>Espécie: <span id="especie"></span></p>
      <p>Dia: <span id="dia"></span> de <span id="total"></span></p>
      <p>Temperatura: <span id="temp"></span> °C</p>
      <p>Umidade: <span id="umid"></span> %</p>
      <p>Ovos: <span id="ovos"></span></p>
      <p>Pintinhos: <span id="pintinhos"></span></p>
      <canvas id="graficoTemp" width="320" height="150"></canvas>
      <canvas id="graficoUmid" width="320" height="150"></canvas>
      <script>
        function carregar() {
          fetch('/json')
            .then(response => response.json())
            .then(d => {
              document.getElementById('especie').textContent = d.especie;
              document.getElementById('dia').textContent = d.diaAtual;
              document.getElementById('total').textContent = d.totalDias;
              document.getElementById('temp').textContent = d.temp.toFixed(1);
              document.getElementById('umid').textContent = d.umid.toFixed(1);
              document.getElementById('ovos').textContent = d.ovos;
              document.getElementById('pintinhos').textContent = d.pintinhos;

              new Chart(document.getElementById('graficoTemp'), {
                type: 'bar',
                data: {
                  labels: ['Temperatura'],
                  datasets: [{
                    label: '°C',
                    data: [d.temp],
                    backgroundColor: 'rgba(255, 99, 132, 0.6)'
                  }]
                }
              });

              new Chart(document.getElementById('graficoUmid'), {
                type: 'bar',
                data: {
                  labels: ['Umidade'],
                  datasets: [{
                    label: '%',
                    data: [d.umid],
                    backgroundColor: 'rgba(54, 162, 235, 0.6)'
                  }]
                }
              });
            });
        }

        carregar();
        setInterval(carregar, 10000); // Atualiza a cada 10s
      </script>
      </body></html>
    )rawliteral";

    request->send(200, "text/html", html);
  });

  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    doc["temp"] = tempAtual;
    doc["umid"] = umidAtual;
    doc["diaAtual"] = diaAtual;
    doc["totalDias"] = totalDias;
    doc["especie"] = especies[especieSelecionada];
    doc["ovos"] = ovosIncubados;
    doc["pintinhos"] = pintinhosNascidos;
    doc["estado"] = estadoSistema;

    String resposta;
    serializeJson(doc, resposta);
    request->send(200, "application/json", resposta);
  });

  server.begin();
}

