/*
  Robô Seguidor de Linha - CDR CAR (Casa da Robótica)
  VERSÃO CORRIGIDA: Lógica IR + Controle PWM/DIR
*/

// ---------- PINOS DOS SENSORES ----------
const int S_ESQ = A0;       
const int S_CEN = A1;       
const int S_DIR = A2;       

// ---------- PINOS DOS MOTORES ----------
const int PWM_ESQ = 10;     
const int DIR_ESQ = 12;     
const int PWM_DIR = 11;     
const int DIR_DIR = 13;     

// ---------- AJUSTES DE VELOCIDADE E LIMIAR ----------
const int VEL_BASE  = 140; // Velocidade em reta (0 a 255)
const int VEL_CURVA = 110; // Velocidade de rotação nas curvas
const int LIMITE_PRETO = 500; // > 500 = Preto | < 500 = Branco

void setup() {
  pinMode(PWM_ESQ, OUTPUT); 
  pinMode(DIR_ESQ, OUTPUT);
  pinMode(PWM_DIR, OUTPUT); 
  pinMode(DIR_DIR, OUTPUT);
  
  // Inicia com motores desligados
  pararTotal();
  delay(1000);
}

void loop() {
  // Leitura analógica dos sensores TCRT5000
  int esq = analogRead(S_ESQ);
  int cen = analogRead(S_CEN);
  int dir = analogRead(S_DIR);

  // Preto absorve luz IR (retorna valor ALTO)
  // Branco reflete luz IR (retorna valor BAIXO)
  bool naLinhaEsq = (esq > LIMITE_PRETO);
  bool naLinhaCen = (cen > LIMITE_PRETO);
  bool naLinhaDir = (dir > LIMITE_PRETO);

  // 1) Sensor central na linha preta: segue reto
  if (naLinhaCen) {
    motores(VEL_BASE, VEL_BASE);
  }
  // 2) Linha desviou para a esquerda: gira para a esquerda
  else if (naLinhaEsq && !naLinhaDir) {
    motores(-VEL_CURVA, VEL_CURVA); 
  }
  // 3) Linha desviou para a direita: gira para a direita
  else if (naLinhaDir && !naLinhaEsq) {
    motores(VEL_CURVA, -VEL_CURVA); 
  }
  // 4) Fora da linha preta (Mesa Branca): para imediatamente
  else {
    pararTotal();
  }
}

// Controle de acionamento dos motores com correção de fase PWM
void motores(int esq, int dir) {
  // --- MOTOR ESQUERDO ---
  if (esq >= 0) {
    digitalWrite(DIR_ESQ, LOW);
    analogWrite(PWM_ESQ, esq);
  } else {
    digitalWrite(DIR_ESQ, HIGH);
    analogWrite(PWM_ESQ, 255 - abs(esq)); // Correção da polaridade da direção
  }

  // --- MOTOR DIREITO ---
  if (dir >= 0) {
    digitalWrite(DIR_DIR, LOW);
    analogWrite(PWM_DIR, dir);
  } else {
    digitalWrite(DIR_DIR, HIGH);
    analogWrite(PWM_DIR, 255 - abs(dir)); // Correção da polaridade da direção
  }
}

// Desliga os motores totalmente
void pararTotal() {
  digitalWrite(DIR_ESQ, LOW);
  digitalWrite(DIR_DIR, LOW);
  analogWrite(PWM_ESQ, 0);
  analogWrite(PWM_DIR, 0);
}