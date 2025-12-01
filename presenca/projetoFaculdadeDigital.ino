// SISTEMA DE CHAMADA BIOMÉTRICA

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include "FS.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

// CONFIGURAÇÕES wifi
const char* ssid       = "onibus";     
const char* password   = "12345678";

// Configuração NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; // UTC-3 (Brasil)
const int   daylightOffset_sec = 0;

// Variável de controle do relógio
bool horarioSincronizado = false;

WebServer server(80);

// Configurações Gerais
const String SENHA_ADMIN = "1234"; 
const char* DB_FILE = "/database.txt"; 
const char* LOG_FILE = "/presenca.csv"; 

#define PINO_RED   4
#define PINO_GREEN 18
#define PINO_BLUE  19

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2); 

const byte LINHAS = 4; 
const byte COLUNAS = 4; 
char keys[LINHAS][COLUNAS] = {
  {'D', 'C', 'B', 'A'},
  {'#', '9', '6', '3'},
  {'0', '8', '5', '2'},
  {'*', '7', '4', '1'}
};
byte pinosLinhas[LINHAS] = {13, 12, 14, 27}; 
byte pinosColunas[COLUNAS] = {26, 25, 33, 32}; 
Keypad teclado = Keypad(makeKeymap(keys), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// Declarações
void definirCor(int r, int g, int b);
bool verificarSenhaAdmin();
void menuAdministrador();
void cadastrarUsuario();
void excluirUsuario();
void excluirTudo();
void modoRelatorioWiFi(); 
void sincronizarHorarioAutomatico(); 
String lerTeclado(String titulo, int maxChars);
char esperarTecla();
int buscarProximoIdLivre();
bool capturarDigital(int id);
void salvarNoBanco(String m, int id);
int buscarIdPorMatricula(String m);
String buscarMatriculaPorId(int id);
void removerDoBanco(String mRemover);
void mensagemErro(String msg);
void mensagemSucesso(String msg1, String msg2);
void registrarPresenca(String matricula);
void setupWebServer();
void verificarDigital();

// Setup
void setup() {
  Serial.begin(115200);
  
  if(!SPIFFS.begin(true)){ Serial.println("Erro SPIFFS"); return; }

  pinMode(PINO_RED, OUTPUT); pinMode(PINO_GREEN, OUTPUT); pinMode(PINO_BLUE, OUTPUT);
  
  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Sistema Iniciado");

  // Inicia Sensor
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  if (finger.verifyPassword()) {
    definirCor(0, 1, 0); delay(500);
  } else {
    lcd.setCursor(0,1); lcd.print("Erro Sensor!");
    definirCor(1, 0, 0); while (1) { delay(1); }
  }
  
  delay(1000); lcd.clear();
}

// Loop Principal
void loop() {
  definirCor(0, 0, 1); // Azul (Espera)
  lcd.setCursor(0, 0); lcd.print("Aproxime Dedo:");

  char tecla = teclado.getKey();
  
  if (tecla == 'A') {
    definirCor(1, 1, 0);
    if (verificarSenhaAdmin()) { menuAdministrador(); }
    lcd.clear(); 
  }

  verificarDigital();
  delay(50);
}


void sincronizarHorarioAutomatico() {
  lcd.clear(); 
  lcd.print("Sincronizando");
  lcd.setCursor(0,1); lcd.print("Aguarde...");
  
  WiFi.begin(ssid, password);
  
  //Tenta Conectar no Wi-Fi
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500); 
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    //Pede a hora
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Tenta obter a hora válida por até 5 segundos
    struct tm timeinfo;
    int retryNTP = 0;
    while (!getLocalTime(&timeinfo) && retryNTP < 10) {
      delay(500);
      retryNTP++;
    }
    
    // Verifica se o ano é válido (maior que 2020)
    if (timeinfo.tm_year > (2020 - 1900)) {
      horarioSincronizado = true;
      lcd.clear(); lcd.print("Hora OK!");
      // Mostra a hora na tela
      lcd.setCursor(0,1); 
      lcd.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      delay(2000); 
    } else {
      lcd.clear(); lcd.print("Erro NTP");
      delay(1000);
    }
    
  } else {
    lcd.clear(); lcd.print("Sem Internet");
    delay(1000);
  }

  // Desliga WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
void verificarDigital() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  finger.image2Tz();
  p = finger.fingerFastSearch();
  
  if (p == FINGERPRINT_OK) {
    int idEncontrado = finger.fingerID;
    String matricula = buscarMatriculaPorId(idEncontrado);

    // Se for a primeira vez e o relógio não estiver ajustado:
    if (!horarioSincronizado) {
      definirCor(1, 1, 0);
      sincronizarHorarioAutomatico();
    }

    registrarPresenca(matricula);

    definirCor(0, 1, 0);
    lcd.clear(); lcd.print("PRESENCA OK!");
    lcd.setCursor(0, 1); lcd.print("Matr:" + matricula);
    delay(3000); lcd.clear();
  } else if (p == FINGERPRINT_NOTFOUND) {
    definirCor(1, 0, 0); 
    lcd.clear(); lcd.print("NAO CADASTRADO");
    for(int i=0; i<3; i++) { digitalWrite(PINO_RED, LOW); delay(100); digitalWrite(PINO_RED, HIGH); delay(100); }
    delay(1000); lcd.clear();
  }
}

// Funções do Sistema

void definirCor(int r, int g, int b) {
  digitalWrite(PINO_RED, r ? HIGH : LOW);
  digitalWrite(PINO_GREEN, g ? HIGH : LOW);
  digitalWrite(PINO_BLUE, b ? HIGH : LOW);
}

bool verificarSenhaAdmin() {
  lcd.clear(); lcd.print("Senha Admin:"); lcd.setCursor(0, 1);
  String s = "";
  while(true) {
    char k = teclado.getKey();
    if(k){ if(k=='#') break; if(k=='*') return false; s+=k; lcd.print('*'); }
  }
  if(s == SENHA_ADMIN) return true;
  else { mensagemErro("Senha Errada"); return false; }
}

void menuAdministrador() {
  while(true) {
    lcd.clear();        lcd.print("A-Add B-Um C-ALL"); 
    lcd.setCursor(0,1); lcd.print("D-Relator *-Sair");
    
    char k = esperarTecla();
    if(k=='A') cadastrarUsuario();
    if(k=='B') excluirUsuario();
    if(k=='C') excluirTudo();
    if(k=='D') modoRelatorioWiFi();
    if(k=='*') return;
  }
}

void modoRelatorioWiFi() {
  lcd.clear(); lcd.print("Ligando WiFi...");
  
  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500); lcd.setCursor(0,1); lcd.print("."); tentativas++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    mensagemErro("Falha Conexao");
    WiFi.disconnect(true);
    return;
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  horarioSincronizado = true;
  
  setupWebServer(); 
  server.begin(); 

  lcd.clear(); lcd.print(WiFi.localIP());
  lcd.setCursor(0,1); lcd.print("[*] p/ Sair");

  while(true) {
    server.handleClient(); 
    // Pisca verde para indicar que servidor está no ar
    definirCor(0, 1, 0); delay(100); definirCor(0, 0, 0); delay(100);
    
    char k = teclado.getKey();
    if (k == '*') {
      server.stop();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      lcd.clear(); lcd.print("WiFi Desligado");
      delay(1000);
      return; 
    }
  }
}

void cadastrarUsuario() {
  String m = lerTeclado("Matricula:", 9); if(m=="") return;
  if(buscarIdPorMatricula(m)!=-1) { mensagemErro("Ja existe!"); return; }
  int id = buscarProximoIdLivre();
  if(id==-1) { mensagemErro("Cheio!"); return; }
  if(capturarDigital(id)) { salvarNoBanco(m, id); mensagemSucesso("OK!", m); }
}

void excluirUsuario() {
  String m = lerTeclado("Excluir:", 9); if(m=="") return;
  int id = buscarIdPorMatricula(m);
  if(id==-1) { mensagemErro("Nao achei"); return; }
  if(finger.deleteModel(id)==FINGERPRINT_OK) { removerDoBanco(m); mensagemSucesso("Excluido!", m); }
  else { mensagemErro("Erro Sensor"); }
}

void excluirTudo() {
  lcd.clear(); lcd.print("Confirmar? [#]");
  if (esperarTecla() != '#') return;
  lcd.clear(); lcd.print("Apagando...");
  finger.emptyDatabase();
  SPIFFS.remove(DB_FILE);
  SPIFFS.remove(LOG_FILE); 
  horarioSincronizado = false; 
  mensagemSucesso("TUDO APAGADO!", "");
}

String lerTeclado(String titulo, int maxChars) {
  lcd.clear(); lcd.print(titulo); lcd.setCursor(0,1);
  String buffer = "";
  while(true) {
    char key = teclado.getKey();
    if(key) {
      if (key == '*') return ""; 
      if (key == '#') return buffer; 
      if (buffer.length() < maxChars) { buffer += key; lcd.print(key); }
    }
  }
}

char esperarTecla() { while(true) { char key = teclado.getKey(); if (key) return key; } }

int buscarProximoIdLivre() { for (int i = 1; i <= 127; i++) { if (finger.loadModel(i) != FINGERPRINT_OK) return i; } return -1; }

bool capturarDigital(int id) {
  lcd.clear(); lcd.print("Coloque o dedo"); lcd.setCursor(0,1); lcd.print("Slot: " + String(id));
  int p = -1;
  while (p != FINGERPRINT_OK) { p = finger.getImage(); if (p != FINGERPRINT_OK && p != FINGERPRINT_NOFINGER) { mensagemErro("Erro Leitura"); return false; } }
  finger.image2Tz(1);
  lcd.clear(); lcd.print("Tire o dedo..."); delay(1000);
  while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); }
  lcd.clear(); lcd.print("Dedo Novamente");
  p = -1; while (p != FINGERPRINT_OK) { p = finger.getImage(); }
  finger.image2Tz(2);
  if (finger.createModel() != FINGERPRINT_OK) { mensagemErro("Nao combinam"); return false; }
  if (finger.storeModel(id) != FINGERPRINT_OK) { mensagemErro("Erro ao Salvar"); return false; }
  return true;
}

void salvarNoBanco(String matricula, int id) {
  File file = SPIFFS.open(DB_FILE, FILE_APPEND);
  if(file){ file.println(matricula + "," + String(id)); file.close(); }
}

int buscarIdPorMatricula(String matriculaBusca) {
  File file = SPIFFS.open(DB_FILE, FILE_READ);
  if(!file) return -1;
  while(file.available()){
    String linha = file.readStringUntil('\n'); linha.trim(); 
    int virgula = linha.indexOf(',');
    if (virgula > 0) {
      String matLida = linha.substring(0, virgula);
      String idLido = linha.substring(virgula + 1);
      if (matLida == matriculaBusca) { file.close(); return idLido.toInt(); }
    }
  } file.close(); return -1;
}

String buscarMatriculaPorId(int idBusca) {
  File file = SPIFFS.open(DB_FILE, FILE_READ);
  if(!file) return "Erro DB";
  while(file.available()){
    String linha = file.readStringUntil('\n'); linha.trim(); 
    int virgula = linha.indexOf(',');
    if (virgula > 0) {
      String matLida = linha.substring(0, virgula);
      int idLido = linha.substring(virgula + 1).toInt();
      if (idLido == idBusca) { file.close(); return matLida; }
    }
  } file.close(); return "Nao Encontrado";
}

void removerDoBanco(String matriculaRemover) {
  File file = SPIFFS.open(DB_FILE, FILE_READ);
  if(!file) return;
  String novoConteudo = "";
  while(file.available()){
    String linha = file.readStringUntil('\n'); linha.trim();
    if (linha.length() > 0) {
      int virgula = linha.indexOf(',');
      String matLida = linha.substring(0, virgula);
      if (matLida != matriculaRemover) { novoConteudo += linha + "\n"; }
    }
  } file.close();
  File fileWrite = SPIFFS.open(DB_FILE, FILE_WRITE);
  if(fileWrite){ fileWrite.print(novoConteudo); fileWrite.close(); }
}

void registrarPresenca(String matricula) {
  struct tm timeinfo;
  String timeString = "Erro Hora";
  
  // Tenta pegar a hora do relógio interno
  if(getLocalTime(&timeinfo)){
    char buffer[50];
    // Formato: DD/MM/AAAA HH:MM:SS
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    timeString = String(buffer);
  } else {
    timeString = "T+" + String(millis()/1000) + "s";
  }

  File file = SPIFFS.open(LOG_FILE, FILE_APPEND);
  if(file){
    file.printf("%s,%s\n", matricula.c_str(), timeString.c_str());
    file.close();
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<style>body{font-family:Arial;text-align:center;}table{margin:auto;border-collapse:collapse;width:80%;}th,td{border:1px solid #ddd;padding:8px;}th{background-color:#04AA6D;color:white;}tr:nth-child(even){background-color:#f2f2f2;}button{background-color:#008CBA;color:white;padding:10px 20px;border:none;cursor:pointer;margin:10px;}.danger{background-color:#f44336;}</style></head><body>";
    html += "<h1>Relatorio</h1>";
    html += "<table><tr><th>Matricula</th><th>Data/Hora</th></tr>";
    File file = SPIFFS.open(LOG_FILE, "r");
    if (file) {
      while(file.available()){
        String linha = file.readStringUntil('\n'); linha.trim();
        if(linha.length()>0) {
          int virgula = linha.indexOf(',');
          if(virgula > 0) {
            String mat = linha.substring(0, virgula);
            String hora = linha.substring(virgula+1);
            html += "<tr><td>" + mat + "</td><td>" + hora + "</td></tr>";
          }
        }
      } file.close();
    }
    html += "</table><br><a href='/'><button>Atualizar</button></a><a href='/download'><button>Baixar CSV</button></a><a href='/limpar'><button class='danger'>Limpar</button></a></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/download", HTTP_GET, []() {
    if (SPIFFS.exists(LOG_FILE)) {
      File file = SPIFFS.open(LOG_FILE, "r");
      server.streamFile(file, "text/csv");
      file.close();
    } else { server.send(404, "text/plain", "Vazio"); }
  });
  
  server.on("/limpar", HTTP_GET, []() {
    SPIFFS.remove(LOG_FILE);
    server.send(200, "text/html", "<h1>Lista Apagada!</h1><a href='/'>Voltar</a>");
  });
}

void mensagemErro(String msg) {
  definirCor(1, 0, 0); lcd.clear(); lcd.print("ERRO:"); lcd.setCursor(0,1); lcd.print(msg); delay(2000);
}
void mensagemSucesso(String msg1, String msg2) {
  definirCor(0, 1, 0); lcd.clear(); lcd.print(msg1); lcd.setCursor(0,1); lcd.print(msg2); delay(2000);
}