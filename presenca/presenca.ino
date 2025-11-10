/*
 * ==========================================================
 * == PROJETO FINAL: LEITOR DE MATRÍCULA COM EEPROM ==
 * ==========================================================
 * Componentes: Arduino Uno, Keypad 4x4
 * Pinagem:
 * - Keypad Pinos 1,2,3,4 -> Arduino D9,D8,D7,D6 (Ordem pode variar)
 * - Keypad Pinos 5,6,7,8 -> Arduino D5,D4,D3,D2 (Ordem pode variar)
 *
 * (Este código usa a pinagem corrigida que encontramos)
 */

#include <Keypad.h>    // Biblioteca para o teclado matricial
#include <EEPROM.h>    // Biblioteca para a memória interna

// --- Configuração do Keypad ---
const byte ROWS = 4; // 4 linhas
const byte COLS = 4; // 4 colunas

// Mapeamento das teclas
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Pinos do Arduino (VERSÃO FINAL CORRIGIDA)
// Nossos testes provaram que esta é a ordem correta para o seu hardware
byte rowPins[ROWS] = {2, 3, 4, 5}; 
byte colPins[COLS] = {6, 7, 8, 9};

// Inicializa o objeto Keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// --- Variáveis Globais ---
String matricula = ""; // String para armazenar a matrícula
int proximoEndereco;   // Ponteiro para o próximo local livre na EEPROM

//================================================
//  SETUP: Executa uma vez
//================================================
void setup() {
  Serial.begin(9600);
  Serial.println("Iniciando leitor de matricula (EEPROM)...");

  // Pega o ponteiro de "próximo endereço" salvo na EEPROM
  EEPROM.get(0, proximoEndereco);

  // Se o valor lido não faz sentido (ex: primeira vez), formata a memória.
  if (proximoEndereco > 1023 || proximoEndereco < 2) {
    Serial.println("EEPROM nao inicializada. Formatando...");
    limparMemoria(); // Limpa tudo e seta proximoEndereco para 2
  } else {
    Serial.print("Memoria iniciada. Proximo endereco livre: ");
    Serial.println(proximoEndereco);
  }
  
  Serial.println("Pronto para digitar.");
  Serial.println("#: Salvar | *: Limpar | A: Ler | D: Apagar Tudo");
}

//================================================
//  LOOP: Executa continuamente
//================================================
void loop() {
  char key = keypad.getKey();

  if (key) {
    Serial.print(key); // mostra tecla pressionada

    // Comandos especiais
    if (key == '#') {
      Serial.print("\nDEBUG matricula atual: '");
      Serial.print(matricula);
      Serial.println("'");

      if (matricula.length() > 0) {
        salvarMatricula();
      } else {
        Serial.println(" [VAZIO] Nenhuma matricula para salvar.");
      }
    }
    else if (key == '*') {
      matricula = "";
      Serial.println(" [LIMPO]");
    }
    else if (key == 'A') {
      Serial.println();
      lerMatriculasSalvas();
    }
    else if (key == 'D') {
      Serial.println();
      Serial.println("TEM CERTEZA? Pressione D novamente para apagar TUDO.");
      char confirm = keypad.waitForKey();
      if (confirm == 'D') {
        limparMemoria();
        Serial.println("MEMORIA EEPROM APAGADA.");
      } else {
        Serial.println("Cancelado.");
      }
    }
    else {
      // Captura números, letras e A–D
      if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D') || (key >= 'a' && key <= 'z')) {
        matricula += key;
        Serial.print(" [matricula agora: ");
        Serial.print(matricula);
        Serial.println("]");
      }
    }
  }
}



//================================================
//  FUNÇÕES AUXILIARES (EEPROM)
//================================================

void salvarMatricula() {

  Serial.print("DEBUG matricula atual: '");
  Serial.print(matricula);
  Serial.println("'");

  // 1. Verifica se a matrícula está vazia
  if (matricula.length() == 0) {
    Serial.println(" [VAZIO] Nenhuma matricula para salvar.");
    return;
  }

  // 2. Verifica se há espaço
  // +1 para o caractere NULO ('\0') que marca o fim da string
  if (proximoEndereco + matricula.length() + 1 > 1023) {
    Serial.println(" [ERRO] Memoria EEPROM cheia!");
    return;
  }

  // 3. Se tudo OK, salva a matrícula
  Serial.print("... Salvando '");
  Serial.print(matricula);
  Serial.print("' no endereco ");
  Serial.println(proximoEndereco);

  // Salva caractere por caractere
  for (int i = 0; i < matricula.length(); i++) {
    EEPROM.put(proximoEndereco + i, matricula[i]);
  }

  // Atualiza o "proximoEndereco"
  proximoEndereco += matricula.length();
  
  // Adiciona o caractere NULO ('\0') para marcar o fim desta matrícula
  EEPROM.put(proximoEndereco, '\0');
  proximoEndereco++; // Incrementa para o próximo espaço livre

  // Salva o novo "proximoEndereco" nos bytes 0 e 1 
  EEPROM.put(0, proximoEndereco);

  Serial.println("Salvo com sucesso!");
  matricula = ""; // Limpa a string para a próxima
}


void lerMatriculasSalvas() {
  Serial.println("--- Lendo Matriculas Salvas na EEPROM ---");
  
  EEPROM.get(0, proximoEndereco); // Pega o ponteiro atualizado

  int addr = 2; // Endereço inicial das matrículas
  
  if (addr == proximoEndereco) {
    Serial.println("(Memoria vazia)");
  }

  // Lê enquanto não chegamos ao fim
  while (addr < proximoEndereco) {
    char c = EEPROM.read(addr); // Lê um byte (caractere)
    
    if (c == '\0') { // Se for o terminador nulo
      Serial.println(); // Pula para a próxima linha
    } else {
      Serial.print(c); // Imprime o caractere
    }
    addr++; // Vai para o próximo endereço
  }
  Serial.println("--- Fim da Leitura ---");
}


void limparMemoria() {
  Serial.println("Formatando EEPROM...");
  for (int i = 0; i < 1024; i++) {
    EEPROM.write(i, 255); // 255 é o valor "padrão" de um byte vazio
  }
  
  // Reseta o nosso ponteiro para o início (endereço 2)
  proximoEndereco = 2; 
  EEPROM.put(0, proximoEndereco); // Salva o ponteiro resetado
  
  Serial.println("Memoria limpa. Proximo endereco: 2");
}