// ---------------------------------------------------------
// Jogo de Reflexo - sem LEDs físicos. A "tela" é o computador:
// ligue este Arduino ao PC e abra o jogo_reflexo.html no
// Chrome ou Edge para ver placar, tempo e as toupeiras.
// Quem mostra qual toupeira apareceu é a tela (campo led= do TICK).
// ---------------------------------------------------------

// ---------- CONFIGURAÇÕES ----------
const uint8_t NUM_TOUPEIRAS = 3;
const uint8_t buttonPins[NUM_TOUPEIRAS] = {A0, A1, A2};
const uint8_t BUZZER_PIN = 8;

const unsigned long GAME_DURATION   = 30000; // duração da partida (ms) -> 30s
const unsigned long WINDOW_START    = 2000;  // janela de reação no início do jogo (ms)
const unsigned long WINDOW_MIN      = 400;   // janela de reação no fim do jogo (ms)

const int   PONTOS_MAX = 100; // pontos por um clique "instantâneo"
const int   PONTOS_MIN = 10;  // pontos por um clique no limite do tempo
const int   PENALIDADE_ERRO = 5; // pontos perdidos ao apertar o botão errado

const unsigned long DEBOUNCE_MS = 30;

// --- sons ---
const unsigned int  BEEP_FREQ           = 1200; // frequência do "pi" de fundo
const unsigned int  BEEP_DURACAO        = 60;   // duração de cada "pi" (ms)
const unsigned long BEEP_INTERVALO_INI  = 600;  // intervalo entre "pi"s no início (ms)
const unsigned long BEEP_INTERVALO_MIN  = 150;  // intervalo entre "pi"s no final (ms)

const unsigned int FREQ_ACERTO  = 1800;
const unsigned int FREQ_ERRO    = 250;
const unsigned int FREQ_TIMEOUT = 200;

// --- protocolo serial (o HTML lê isso) ---
const unsigned long TICK_INTERVAL = 150; // de quanto em quanto tempo manda um "retrato" do jogo (ms)
unsigned long ultimoTick = 0;

// ---------- ESTADOS DO JOGO ----------
enum EstadoJogo { IDLE, RODADA_ATIVA, TROCA_RODADA, FIM_DE_JOGO };
EstadoJogo estado = IDLE;

int score = 0;
int rodadas = 0;

unsigned long janelaAtual = WINDOW_START;
unsigned long inicioRodada = 0;
unsigned long inicioJogo = 0;
int toupeiraAtiva = -1; // qual toupeira está de fora (-1 = nenhuma)

unsigned long ultimoDebounce[NUM_TOUPEIRAS] = {0, 0, 0};
bool estadoBotaoAnterior[NUM_TOUPEIRAS] = {HIGH, HIGH, HIGH};
bool estadoBotaoEstavel[NUM_TOUPEIRAS]  = {HIGH, HIGH, HIGH};

unsigned long tempoTrocaRodada = 0;
unsigned long ultimoBeepFundo = 0;

void setup() {
  for (uint8_t i = 0; i < NUM_TOUPEIRAS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(115200); // o Serial é a "tela" do jogo

  randomSeed(analogRead(A4)); // pino analógico livre, sem ligação, dá boa aleatoriedade

  enviarEvento(F("IDLE"));
}

void loop() {
  // manda um "retrato" do jogo pro computador (placar, tempo, toupeira ativa)
  enviarTick(false);

  // "pi pi pi" de fundo, fica mais rápido com o tempo
  atualizarBeepDeFundo();

  // Checagem de fim de jogo em PRIMEIRO lugar: se o tempo acabou, encerra já.
  if ((estado == RODADA_ATIVA || estado == TROCA_RODADA) &&
      (millis() - inicioJogo >= GAME_DURATION)) {
    finalizarJogo();
  }

  switch (estado) {
    case IDLE:
      if (algumBotaoApertado() >= 0) {
        iniciarJogo();
      }
      break;

    case RODADA_ATIVA:
      rodarRodadaAtiva();
      break;

    case TROCA_RODADA:
      // pequena pausa entre rodadas (nenhuma toupeira de fora) antes da próxima
      if (millis() - tempoTrocaRodada > 200) {
        novaRodada();
      }
      break;

    case FIM_DE_JOGO:
      if (algumBotaoApertado() >= 0) {
        estado = IDLE;
        enviarEvento(F("IDLE"));
      }
      break;
  }
}

// ---------------- FUNÇÕES DE JOGO ----------------

void iniciarJogo() {
  score = 0;
  rodadas = 0;
  toupeiraAtiva = -1;
  tocarBuzina();
  contagemRegressiva();
  inicioJogo = millis();
  novaRodada();
}

// Janela de reação: no começo é WINDOW_START (mais fácil), no final
// é WINDOW_MIN (mais difícil). É isso que faz o jogo ir acelerando.
unsigned long calcularJanelaAtual() {
  unsigned long decorridoJogo = millis() - inicioJogo;
  if (decorridoJogo > GAME_DURATION) decorridoJogo = GAME_DURATION;

  long janela = map((long)decorridoJogo, 0, (long)GAME_DURATION,
                    (long)WINDOW_START, (long)WINDOW_MIN);
  return (unsigned long)janela;
}

// Mesma lógica, mas pro intervalo do "pi pi pi" de fundo.
unsigned long calcularBeepIntervalo() {
  unsigned long decorridoJogo = millis() - inicioJogo;
  if (decorridoJogo > GAME_DURATION) decorridoJogo = GAME_DURATION;

  long intervalo = map((long)decorridoJogo, 0, (long)GAME_DURATION,
                       (long)BEEP_INTERVALO_INI, (long)BEEP_INTERVALO_MIN);
  return (unsigned long)intervalo;
}

void atualizarBeepDeFundo() {
  if (estado == RODADA_ATIVA || estado == TROCA_RODADA) {
    unsigned long intervalo = calcularBeepIntervalo();
    if (millis() - ultimoBeepFundo >= intervalo) {
      tone(BUZZER_PIN, BEEP_FREQ, BEEP_DURACAO);
      ultimoBeepFundo = millis();
    }
  }
}

void novaRodada() {
  janelaAtual = calcularJanelaAtual();
  toupeiraAtiva = random(0, NUM_TOUPEIRAS);
  inicioRodada = millis();
  estado = RODADA_ATIVA;
  enviarTick(true);
}

void rodarRodadaAtiva() {
  unsigned long decorrido = millis() - inicioRodada;

  // estourou o tempo?
  if (decorrido > janelaAtual) {
    tone(BUZZER_PIN, FREQ_TIMEOUT, 200);
    Serial.println(F("TIMEOUT"));
    toupeiraAtiva = -1;
    iniciarTrocaRodada();
    return;
  }

  // checa cada botão
  for (uint8_t i = 0; i < NUM_TOUPEIRAS; i++) {
    if (botaoFoiPressionado(i)) {
      if ((int)i == toupeiraAtiva) {
        // ACERTOU
        int pontos = calcularPontos(decorrido);
        score += pontos;
        rodadas++;
        tone(BUZZER_PIN, FREQ_ACERTO, 100);

        Serial.print(F("HIT points="));
        Serial.println(pontos);

        toupeiraAtiva = -1;
        iniciarTrocaRodada();
      } else {
        // ERROU o botão
        score -= PENALIDADE_ERRO;
        if (score < 0) score = 0;
        tone(BUZZER_PIN, FREQ_ERRO, 150);
        Serial.println(F("MISS"));
        enviarTick(true); // a toupeira continua de fora, só atualiza o placar
      }
      return; // trata só um botão por vez
    }
  }
}

void iniciarTrocaRodada() {
  tempoTrocaRodada = millis();
  estado = TROCA_RODADA;
  enviarTick(true);
}

int calcularPontos(unsigned long tempoReacaoMs) {
  // Quanto menor o tempo de reação, mais pontos.
  long pontos = map((long)tempoReacaoMs, 0, (long)janelaAtual, PONTOS_MAX, PONTOS_MIN);
  pontos = constrain(pontos, PONTOS_MIN, PONTOS_MAX);
  return (int)pontos;
}

void finalizarJogo() {
  toupeiraAtiva = -1;
  estado = FIM_DE_JOGO;
  tocarSomFim();

  Serial.print(F("GAMEOVER score="));
  Serial.println(score);
}

// ---------------- SONS ----------------

void tocarBuzina() {
  tone(BUZZER_PIN, 500, 200);
  delay(250);
  tone(BUZZER_PIN, 500, 200);
  delay(250);
}

void contagemRegressiva() {
  for (int n = 3; n >= 1; n--) {
    Serial.print(F("COUNTDOWN n="));
    Serial.println(n);
    tone(BUZZER_PIN, 800, 150);
    delay(700);
  }
  Serial.println(F("GO"));
  tone(BUZZER_PIN, 1500, 300);
  delay(400);
}

void tocarSomFim() {
  tone(BUZZER_PIN, 600, 150);
  delay(180);
  tone(BUZZER_PIN, 800, 150);
  delay(180);
  tone(BUZZER_PIN, 1000, 300);
  delay(320);
}

// ---------------- SERIAL (a "tela" do jogo) ----------------

// Manda uma linha simples tipo: EVENT nome
void enviarEvento(const __FlashStringHelper* nome) {
  Serial.print(F("EVENT "));
  Serial.println(nome);
}

// Manda um "retrato" do jogo: TICK state=... score=... time=... led=...
// (o campo continua se chamando "led" porque o HTML lê esse nome;
// agora ele só indica qual toupeira está de fora)
void enviarTick(bool forcar) {
  if (!forcar && millis() - ultimoTick < TICK_INTERVAL) return;

  long restanteMs;
  if (estado == RODADA_ATIVA || estado == TROCA_RODADA) {
    restanteMs = (long)GAME_DURATION - (long)(millis() - inicioJogo);
    if (restanteMs < 0) restanteMs = 0;
  } else {
    restanteMs = GAME_DURATION;
  }
  int segundos = restanteMs / 1000;

  Serial.print(F("TICK state="));
  switch (estado) {
    case IDLE:          Serial.print(F("IDLE"));     break;
    case RODADA_ATIVA:  Serial.print(F("PLAYING"));  break;
    case TROCA_RODADA:  Serial.print(F("PLAYING"));  break;
    case FIM_DE_JOGO:   Serial.print(F("GAMEOVER")); break;
  }
  Serial.print(F(" score="));
  Serial.print(score);
  Serial.print(F(" time="));
  Serial.print(segundos);
  Serial.print(F(" led="));
  Serial.println(toupeiraAtiva);

  ultimoTick = millis();
}

// ---------------- BOTÕES ----------------

bool botaoFoiPressionado(uint8_t i) {
  bool leitura = digitalRead(buttonPins[i]); // LOW = pressionado (INPUT_PULLUP)
  bool pressionadoAgora = false;

  // toda vez que a leitura bruta mudar, reinicia a contagem do debounce
  if (leitura != estadoBotaoAnterior[i]) {
    ultimoDebounce[i] = millis();
  }

  // só considera a leitura "estável" depois do tempo de debounce
  if ((millis() - ultimoDebounce[i]) > DEBOUNCE_MS) {
    if (leitura != estadoBotaoEstavel[i]) {
      estadoBotaoEstavel[i] = leitura;
      if (estadoBotaoEstavel[i] == LOW) {
        pressionadoAgora = true; // borda de descida = acabou de apertar
      }
    }
  }

  estadoBotaoAnterior[i] = leitura;
  return pressionadoAgora;
}

int algumBotaoApertado() {
  for (uint8_t i = 0; i < NUM_TOUPEIRAS; i++) {
    if (botaoFoiPressionado(i)) return i;
  }
  return -1;
}
