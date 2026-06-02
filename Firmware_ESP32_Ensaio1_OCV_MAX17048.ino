/*
 ESP32 + INA219 + ADS1115 + DS18B20 + MAX17048
 - SoC0 automático por OCV no setup (interruptor OFF)
 - OCV->SoC0 por tabela (LUT) + interpolação linear
 - SoC por Coulomb Counting acumulado (Ah_acc)
 - CSV com header na 1ª linha
 - MAX17048 no mesmo I2C (SDA/SCL partilhados)
 - SoC_max guardado em 0..1 (para bater direto com SoC/SoC_ref)
 Bibliotecas:
 - Adafruit INA219
 - Adafruit ADS1X15
 - OneWire
 - DallasTemperature
 - Adafruit MAX1704X
*/

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_MAX1704X.h>

// ===== Pinos ESP32 =====
static const int I2C_SDA = 21;
static const int I2C_SCL = 22;
static const int ONEWIRE_PIN = 4;

// ===== Endereços I2C =====
static const uint8_t INA_ADDR = 0x40;
static const uint8_t ADS_ADDR = 0x48; // 0x49 se o ADS estiver nesse endereço
// MAX17048 normalmente em 0x36 (a biblioteca trata do endereço internamente)

// ===== Divisor tensão =====
static const float DIV_FACTOR = 2.0f; // R1=R2=100k => Vbat = 2*Vmeas

// ===== Logging =====
static const uint32_t SAMPLE_MS = 500;
static const uint32_t RUN_SECONDS = 0; // 0=infinito

// ===== SoC =====
static const float C_AH = 2.6f;

// Queremos I_discharge_A > 0 quando está a descarregar.
// Se no teu log a descarga aparece com I_mA positivo (~+180 mA), deixa +1.
// Se aparecer negativo (~-180 mA), põe -1.
static const int CURRENT_SIGN = +1;

// ===== OCV setup =====
static const uint32_t OCV_REST_MS = 5000;   // repouso antes de medir
static const int OCV_SAMPLES = 40;          // nº amostras para média
static const uint16_t OCV_SAMPLE_MS = 25;   // intervalo entre amostras (ms)

// Sensores
Adafruit_INA219 ina219;
Adafruit_ADS1115 ads;
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

// MAX17048
Adafruit_MAX17048 maxlipo;
bool max_ok = false;

// Tempo e estado
uint32_t t0_ms = 0;
uint32_t last_ms = 0;
float soc0 = 1.0f;     // SoC inicial por OCV (LUT)
float soc  = 1.0f;     // SoC atual
float ah_acc = 0.0f;   // Ah acumulados (descarga positiva)

// --------- LUT OCV -> SoC (aprox.; ajustável) ---------
static const int LUT_N = 11;
static const float OCV_LUT[LUT_N] = {
  3.00f, 3.30f, 3.50f, 3.60f, 3.70f, 3.75f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f
};

static const float SOC_LUT[LUT_N] = {
  0.00f, 0.05f, 0.15f, 0.25f, 0.40f, 0.50f, 0.60f, 0.75f, 0.85f, 0.95f, 1.00f
};

static float socFromOcvLut(float ocv) {
  if (ocv <= OCV_LUT[0]) return SOC_LUT[0];
  if (ocv >= OCV_LUT[LUT_N - 1]) return SOC_LUT[LUT_N - 1];

  for (int i = 0; i < LUT_N - 1; i++) {
    float x0 = OCV_LUT[i], x1 = OCV_LUT[i + 1];
    if (ocv >= x0 && ocv <= x1) {
      float y0 = SOC_LUT[i], y1 = SOC_LUT[i + 1];
      float t = (ocv - x0) / (x1 - x0);
      return y0 + t * (y1 - y0);
    }
  }
  return 0.0f;
}

static void printCsvHeader() {
  // Mantém as tuas colunas e adiciona MAX no fim
  // SoC_max aqui é 0..1
  Serial.println("t_s,Vbus_V,Vshunt_mV,I_mA,P_mW,Vmeas_V,Vbat_V,T_C,SoC,SOC0_OCV,Ah_acc,SoC_max,Vcell_max_V,MAX_ok");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ina219.begin()) {
    Serial.println("ERRO: INA219 nao encontrado (I2C).");
    while (true) delay(1000);
  }

  if (!ads.begin(ADS_ADDR)) {
    Serial.println("ERRO: ADS1115 nao encontrado no endereco configurado.");
    while (true) delay(1000);
  }

  ads.setGain(GAIN_ONE); // ±4.096V
  sensors.begin();

  // MAX17048 (não bloqueia o firmware se falhar)
  max_ok = maxlipo.begin();
  if (max_ok) {
    Serial.println("# MAX17048 OK (I2C)");
    // opcional:
    maxlipo.quickStart();
    delay(50);
  } else {
    Serial.println("# AVISO: MAX17048 nao encontrado (I2C). Vai logar NaN e MAX_ok=0.");
  }

  // ===== Medição OCV (interruptor OFF) =====
  Serial.println("# ===== MEDINDO OCV (interruptor OFF) =====");
  delay(OCV_REST_MS);

  long raw_sum = 0;
  for (int k = 0; k < OCV_SAMPLES; k++) {
    raw_sum += ads.readADC_SingleEnded(0);
    delay(OCV_SAMPLE_MS);
  }

  float raw_avg = (float)raw_sum / (float)OCV_SAMPLES;
  float vmeas_setup = ads.computeVolts((int16_t)raw_avg);
  float ocv_V = vmeas_setup * DIV_FACTOR;

  soc0 = socFromOcvLut(ocv_V);
  soc = soc0;
  ah_acc = 0.0f;

  Serial.print("# OCV=");
  Serial.print(ocv_V, 3);
  Serial.print("V -> SoC0_OCV=");
  Serial.println(soc0, 3);

  t0_ms = millis();
  last_ms = t0_ms;

  // 1ª linha: header
  printCsvHeader();
}

void loop() {
  uint32_t now = millis();

  // stop automático
  if (RUN_SECONDS > 0) {
    float t_s_stop = (now - t0_ms) / 1000.0f;
    if (t_s_stop >= (float)RUN_SECONDS) {
      Serial.println("#STOP");
      while (true) delay(1000);
    }
  }

  // amostragem
  static uint32_t last_print = 0;
  if (now - last_print < SAMPLE_MS) return;
  last_print = now;

  float dt_s = (now - last_ms) / 1000.0f;
  last_ms = now;

  // INA219
  float vbus_V = ina219.getBusVoltage_V();
  float vshunt_mV = ina219.getShuntVoltage_mV();
  float i_mA_raw = ina219.getCurrent_mA();
  float p_mW = ina219.getPower_mW();

  // Corrente de descarga positiva
  float i_dis_A = (i_mA_raw / 1000.0f) * (float)CURRENT_SIGN;

  // ADS1115
  int16_t raw = ads.readADC_SingleEnded(0);
  float vmeas_V = ads.computeVolts(raw);
  float vbat_V = vmeas_V * DIV_FACTOR;

  // DS18B20
  sensors.requestTemperatures();
  float t_C = sensors.getTempCByIndex(0);

  // Coulomb Counting (acumulado)
  float dAh = i_dis_A * (dt_s / 3600.0f);
  ah_acc += dAh;

  soc = soc0 - (ah_acc / C_AH);
  soc = constrain(soc, 0.0f, 1.0f);

  // MAX17048 (se falhar, loga NaN e MAX_ok=0)
  float soc_max = NAN;       // 0..1
  float vcell_max_V = NAN;   // V
  uint8_t max_ok_u8 = 0;

  if (max_ok) {
    soc_max = maxlipo.cellPercent() / 100.0f; // 0..1
    vcell_max_V = maxlipo.cellVoltage();      // V
    max_ok_u8 = 1;
  }

  float t_s = (now - t0_ms) / 1000.0f;

  // CSV
  Serial.print(t_s, 3);           Serial.print(",");
  Serial.print(vbus_V, 4);        Serial.print(",");
  Serial.print(vshunt_mV, 3);     Serial.print(",");
  Serial.print(i_mA_raw, 3);      Serial.print(",");
  Serial.print(p_mW, 3);          Serial.print(",");
  Serial.print(vmeas_V, 4);       Serial.print(",");
  Serial.print(vbat_V, 4);        Serial.print(",");
  Serial.print(t_C, 2);           Serial.print(",");
  Serial.print(soc, 5);           Serial.print(",");
  Serial.print(soc0, 5);          Serial.print(",");
  Serial.print(ah_acc, 6);        Serial.print(",");

  // MAX no fim
  if (isnan(soc_max)) Serial.print("nan"); else Serial.print(soc_max, 6);
  Serial.print(",");
  if (isnan(vcell_max_V)) Serial.print("nan"); else Serial.print(vcell_max_V, 4);
  Serial.print(",");
  Serial.println(max_ok_u8);
}
