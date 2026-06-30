/*
 ESP32 + INA219 + ADS1115 + DS18B20 + MAX17048 + DT(SoC)

 Versão final operacional (DT embebida exportada de dt_soc_light_depth6.pkl):
 - mantém aquisição de sensores, OCV inicial e logging CSV
 - mantém SoC_fw por Coulomb Counting (opcional, para comparação)
 - acrescenta SoC_ml por Árvore de Decisão
 - separa a corrente usada no Coulomb Counting da corrente usada no modelo

 LUT OCV->SoC:
 - pontos discretizados manualmente a partir da curva OCV de 25 °C do documento técnico da LG 18650HG2
 - usada apenas para inicialização de SOC0_OCV
 DT embebida:
 - árvore depth=6 extraída do modelo dt_soc_light_depth6.pkl e integrada na função socModelPredict()
*/

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_MAX1704X.h>
#include <math.h>

// ============================================================
// 1) CONFIGURAÇÃO GERAL
// ============================================================

static const int I2C_SDA = 21;
static const int I2C_SCL = 22;
static const int ONEWIRE_PIN = 4;

static const uint8_t ADS_ADDR = 0x48;

// Divisor resistivo para Vbat
static const float DIV_FACTOR = 2.0f;

// Logging / amostragem
static const uint32_t SAMPLE_MS = 500;
static const uint32_t RUN_SECONDS = 0;  // 0 = infinito

// Capacidade para Coulomb Counting
static const float C_REF_AH = 2.6f; // capacidade de referência atual para Coulomb Counting

// Sinal para Coulomb Counting:
// +1 se descarga já aparece positiva
// -1 se descarga aparece negativa e precisa de inversão
static const int CURRENT_SIGN_CC = +1;

// OCV no arranque
static const uint32_t OCV_REST_MS = 5000;
static const int OCV_SAMPLES = 40;
static const uint16_t OCV_SAMPLE_MS = 25;

// ============================================================
// 2) OBJETOS DOS SENSORES
// ============================================================

Adafruit_INA219 ina219;
Adafruit_ADS1115 ads;
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_MAX17048 maxlipo;

bool max_ok = false;

// ============================================================
// 3) ESTADO DO SISTEMA
// ============================================================

uint32_t t0_ms = 0;
uint32_t last_ms = 0;
uint32_t last_print = 0;

float soc0 = 1.0f;
float soc_fw = 1.0f;
float soc_ml = NAN;
float ah_acc = 0.0f;

// Estado anterior para derivadas da DT
float prev_v_model_V = NAN;
float prev_i_model_A = NAN;
float prev_t_model_s = NAN;

// ============================================================
// 4) LUT OCV -> SoC
// ============================================================

static const int LUT_N = 11;
static const float OCV_LUT[LUT_N] = { 3.10f, 3.33f, 3.47f, 3.57f, 3.65f, 3.73f, 3.80f, 3.90f, 4.00f, 4.10f, 4.17f };
static const float SOC_LUT[LUT_N] = { 0.08f, 0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f, 0.95f, 1.00f };

static float socFromOcvLut(float ocv) {
  if (ocv <= OCV_LUT[0]) return SOC_LUT[0];
  if (ocv >= OCV_LUT[LUT_N - 1]) return SOC_LUT[LUT_N - 1];

  for (int i = 0; i < LUT_N - 1; i++) {
    float x0 = OCV_LUT[i];
    float x1 = OCV_LUT[i + 1];
    if (ocv >= x0 && ocv <= x1) {
      float y0 = SOC_LUT[i];
      float y1 = SOC_LUT[i + 1];
      float t = (ocv - x0) / (x1 - x0);
      return y0 + t * (y1 - y0);
    }
  }
  return 0.0f;
}

// ============================================================
// 5) FUNÇÃO DO MODELO DT
// ============================================================

/*
 A DT treinada no notebook usa as features:
 - voltage
 - current
 - temperature
 - dv_dt
 - di_dt

 Aqui, o modelo deve receber:
 - voltage_V  -> vbat_V
 - current_A  -> current_model_A (mesma convenção do treino)
 - temp_C     -> t_C
 - dv_dt      -> dV_dt
 - di_dt      -> dI_dt


*/
/*
  DT convertida de dt_soc_light_depth6.pkl para lógica C/C++.
 Modelo original: DecisionTreeRegressor(max_depth=6, random_state=42).
 Features do treino, nesta ordem: voltage, current, temperature, dv_dt, di_dt.
 Nota: o ESP32 não carrega o .pkl; a árvore treinada foi convertida para lógica C/C++.
*/
static float socModelPredict(float voltage_V, float current_A, float temp_C, float dv_dt, float di_dt) {
  const float voltage = voltage_V;
  const float current = current_A;
  const float temperature = temp_C;
  if (voltage <= 3.637809992f) {
    if (voltage <= 3.496554971f) {
      if (current <= -2.228445053f) {
        if (voltage <= 3.224105000f) {
          if (temperature <= 0.052579999f) {
            if (current <= -5.926764965f) {
              return 0.666921687f;
            } else {
              return 0.833359565f;
            }
          } else {
            if (current <= -8.626440048f) {
              return 0.823348094f;
            } else {
              return 0.928553295f;
            }
          }
        } else {
          if (temperature <= -0.473209992f) {
            if (temperature <= -16.036680222f) {
              return 0.383965462f;
            } else {
              return 0.578712891f;
            }
          } else {
            if (current <= -6.940739870f) {
              return 0.595030453f;
            } else {
              return 0.774904134f;
            }
          }
        }
      } else {
        if (current <= -0.144304998f) {
          if (current <= -0.154524997f) {
            if (voltage <= 3.418959975f) {
              return 0.886479691f;
            } else {
              return 0.785498844f;
            }
          } else {
            if (temperature <= -14.354139328f) {
              return 0.880314889f;
            } else {
              return 0.162894344f;
            }
          }
        } else {
          if (temperature <= 12.566445351f) {
            if (voltage <= 3.001205087f) {
              return 0.098601888f;
            } else {
              return 0.950714603f;
            }
          } else {
            if (temperature <= 23.818409920f) {
              return 0.374814430f;
            } else {
              return 0.887962806f;
            }
          }
        }
      }
    } else {
      if (current <= -1.602690041f) {
        if (temperature <= -0.683529973f) {
          if (temperature <= -9.832320213f) {
            if (temperature <= -19.506910324f) {
              return 0.058947817f;
            } else {
              return 0.255565347f;
            }
          } else {
            if (current <= -3.680445075f) {
              return 0.256400184f;
            } else {
              return 0.477361163f;
            }
          }
        } else {
          if (current <= -4.963875055f) {
            if (temperature <= 9.201370239f) {
              return 0.309900573f;
            } else {
              return 0.471467509f;
            }
          } else {
            if (voltage <= 3.556335092f) {
              return 0.653153656f;
            } else {
              return 0.559333800f;
            }
          }
        }
      } else {
        if (temperature <= -18.455320358f) {
          if (current <= -0.148134999f) {
            if (current <= -0.855619997f) {
              return 0.482545396f;
            } else {
              return 0.716081451f;
            }
          } else {
            if (voltage <= 3.608479977f) {
              return 0.947620583f;
            } else {
              return 0.758323112f;
            }
          }
        } else {
          if (current <= 0.241365001f) {
            if (voltage <= 3.567269921f) {
              return 0.723622010f;
            } else {
              return 0.646038781f;
            }
          } else {
            if (voltage <= 3.560845017f) {
              return 0.830824534f;
            } else {
              return 0.744427227f;
            }
          }
        }
      }
    }
  } else {
    if (voltage <= 3.806205034f) {
      if (current <= -0.887544990f) {
        if (temperature <= 8.991055012f) {
          if (current <= -2.241214991f) {
            if (temperature <= -0.578369975f) {
              return 0.191120930f;
            } else {
              return 0.281170203f;
            }
          } else {
            if (temperature <= -17.298580170f) {
              return 0.221786054f;
            } else {
              return 0.381915102f;
            }
          }
        } else {
          if (current <= -4.166995049f) {
            if (current <= -7.147619963f) {
              return 0.225153477f;
            } else {
              return 0.362324134f;
            }
          } else {
            if (voltage <= 3.720684886f) {
              return 0.513157456f;
            } else {
              return 0.421980394f;
            }
          }
        }
      } else {
        if (voltage <= 3.738440037f) {
          if (current <= 0.629590005f) {
            if (temperature <= 10.252960205f) {
              return 0.567304491f;
            } else {
              return 0.523711619f;
            }
          } else {
            if (temperature <= -8.885900021f) {
              return 0.759203102f;
            } else {
              return 0.620531094f;
            }
          }
        } else {
          if (current <= 0.731755018f) {
            if (dv_dt <= 0.051814914f) {
              return 0.478356634f;
            } else {
              return 0.355633148f;
            }
          } else {
            if (temperature <= -8.149789810f) {
              return 0.618906872f;
            } else {
              return 0.521073475f;
            }
          }
        }
      }
    } else {
      if (voltage <= 4.185889959f) {
        if (current <= -0.162184998f) {
          if (voltage <= 3.918564916f) {
            if (temperature <= 22.977140427f) {
              return 0.223700704f;
            } else {
              return 0.310566312f;
            }
          } else {
            if (voltage <= 4.076674938f) {
              return 0.172012366f;
            } else {
              return 0.070243066f;
            }
          }
        } else {
          if (voltage <= 4.098745108f) {
            if (voltage <= 4.096050024f) {
              return 0.315274534f;
            } else {
              return 0.696836418f;
            }
          } else {
            if (current <= -0.144304998f) {
              return 0.710960620f;
            } else {
              return 0.165672882f;
            }
          }
        }
      } else {
        if (di_dt <= -0.007618622f) {
          if (current <= 0.884999990f) {
            if (temperature <= -10.252954960f) {
              return 0.057362549f;
            } else {
              return 0.017147836f;
            }
          } else {
            if (temperature <= -18.665639877f) {
              return 0.419271674f;
            } else {
              return 0.124787490f;
            }
          }
        } else {
          if (dv_dt <= 0.000443935f) {
            if (dv_dt <= -0.000137536f) {
              return 0.042851904f;
            } else {
              return 0.792409271f;
            }
          } else {
            if (current <= 1.165955007f) {
              return 0.033520118f;
            } else {
              return 0.144402169f;
            }
          }
        }
      }
    }
  }
}


// ============================================================
// 6) CSV
// ============================================================

static void printCsvHeader() {
  Serial.println(
    "t_s,"
    "Vbus_V,Vshunt_mV,I_mA,P_mW,"
    "Vmeas_V,Vbat_V,T_C,"
    "I_model_A,I_cc_A,"
    "dV_dt,dI_dt,"
    "SoC_fw,SOC0_OCV,Ah_acc,"
    "SoC_ml,"
    "SoC_max,Vcell_max_V,MAX_ok"
  );
}

// ============================================================
// 7) SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ina219.begin()) {
    Serial.println("ERRO: INA219 nao encontrado.");
    while (true) delay(1000);
  }

  if (!ads.begin(ADS_ADDR)) {
    Serial.println("ERRO: ADS1115 nao encontrado.");
    while (true) delay(1000);
  }

  ads.setGain(GAIN_ONE);
  sensors.begin();

  max_ok = maxlipo.begin();
  if (max_ok) {
    Serial.println("# MAX17048 OK");
    maxlipo.quickStart();
    delay(50);
  } else {
    Serial.println("# AVISO: MAX17048 nao encontrado.");
  }

  // OCV inicial
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
  soc_fw = soc0;
  soc_ml = NAN;
  ah_acc = 0.0f;

  Serial.print("# OCV=");
  Serial.print(ocv_V, 3);
  Serial.print("V -> SoC0_OCV=");
  Serial.println(soc0, 3);

  t0_ms = millis();
  last_ms = t0_ms;
  last_print = 0;

  prev_v_model_V = NAN;
  prev_i_model_A = NAN;
  prev_t_model_s = NAN;

  printCsvHeader();
}

// ============================================================
// 8) LOOP PRINCIPAL
// ============================================================

void loop() {
  uint32_t now = millis();

  if (RUN_SECONDS > 0) {
    float t_s_stop = (now - t0_ms) / 1000.0f;
    if (t_s_stop >= (float)RUN_SECONDS) {
      Serial.println("#STOP");
      while (true) delay(1000);
    }
  }

  if (now - last_print < SAMPLE_MS) return;
  last_print = now;

  float dt_s = (now - last_ms) / 1000.0f;
  last_ms = now;

  // ----------------------------------------------------------
  // 8.1) Leitura INA219
  // ----------------------------------------------------------
  float vbus_V = ina219.getBusVoltage_V();
  float vshunt_mV = ina219.getShuntVoltage_mV();
  float i_mA_raw = ina219.getCurrent_mA();
  float p_mW = ina219.getPower_mW();

  // Corrente para o modelo: usar convenção direta do sensor/log
  float current_model_A = i_mA_raw / 1000.0f;

  // Corrente para Coulomb Counting: permitir conversão para descarga positiva
  float current_cc_A = current_model_A * (float)CURRENT_SIGN_CC;

  // ----------------------------------------------------------
  // 8.2) Leitura ADS1115
  // ----------------------------------------------------------
  int16_t raw = ads.readADC_SingleEnded(0);
  float vmeas_V = ads.computeVolts(raw);
  float vbat_V = vmeas_V * DIV_FACTOR;

  // ----------------------------------------------------------
  // 8.3) Leitura DS18B20
  // ----------------------------------------------------------
  sensors.requestTemperatures();
  float t_C = sensors.getTempCByIndex(0);

  // ----------------------------------------------------------
  // 8.4) SoC_fw por Coulomb Counting
  // ----------------------------------------------------------
  float dAh = current_cc_A * (dt_s / 3600.0f);
  ah_acc += dAh;
  soc_fw = soc0 - (ah_acc / C_REF_AH);
  soc_fw = constrain(soc_fw, 0.0f, 1.0f);

  // ----------------------------------------------------------
  // 8.5) Features da DT
  // ----------------------------------------------------------
  float t_s = (now - t0_ms) / 1000.0f;
  float dV_dt = NAN;
  float dI_dt = NAN;

  if (!isnan(prev_v_model_V) && !isnan(prev_i_model_A) && !isnan(prev_t_model_s)) {
    float dt_feat = t_s - prev_t_model_s;
    if (dt_feat > 1e-6f) {
      dV_dt = (vbat_V - prev_v_model_V) / dt_feat;
      dI_dt = (current_model_A - prev_i_model_A) / dt_feat;
    }
  }

  prev_v_model_V = vbat_V;
  prev_i_model_A = current_model_A;
  prev_t_model_s = t_s;

  // ----------------------------------------------------------
  // 8.6) SoC_ml por DT
  // ----------------------------------------------------------
  soc_ml = socModelPredict(vbat_V, current_model_A, t_C, dV_dt, dI_dt);
  if (!isnan(soc_ml)) {
    soc_ml = constrain(soc_ml, 0.0f, 1.0f);
  }

  // ----------------------------------------------------------
  // 8.7) MAX17048
  // ----------------------------------------------------------
  float soc_max = NAN;
  float vcell_max_V = NAN;
  uint8_t max_ok_u8 = 0;

  if (max_ok) {
    soc_max = maxlipo.cellPercent() / 100.0f;
    vcell_max_V = maxlipo.cellVoltage();
    max_ok_u8 = 1;
  }

  // ----------------------------------------------------------
  // 8.8) CSV
  // ----------------------------------------------------------
  Serial.print(t_s, 3);             Serial.print(",");
  Serial.print(vbus_V, 4);          Serial.print(",");
  Serial.print(vshunt_mV, 3);       Serial.print(",");
  Serial.print(i_mA_raw, 3);        Serial.print(",");
  Serial.print(p_mW, 3);            Serial.print(",");
  Serial.print(vmeas_V, 4);         Serial.print(",");
  Serial.print(vbat_V, 4);          Serial.print(",");
  Serial.print(t_C, 2);             Serial.print(",");

  Serial.print(current_model_A, 6); Serial.print(",");
  Serial.print(current_cc_A, 6);    Serial.print(",");

  if (isnan(dV_dt)) Serial.print("nan"); else Serial.print(dV_dt, 6);
  Serial.print(",");
  if (isnan(dI_dt)) Serial.print("nan"); else Serial.print(dI_dt, 6);
  Serial.print(",");

  Serial.print(soc_fw, 5);          Serial.print(",");
  Serial.print(soc0, 5);            Serial.print(",");
  Serial.print(ah_acc, 6);          Serial.print(",");

  if (isnan(soc_ml)) Serial.print("nan"); else Serial.print(soc_ml, 6);
  Serial.print(",");

  if (isnan(soc_max)) Serial.print("nan"); else Serial.print(soc_max, 6);
  Serial.print(",");
  if (isnan(vcell_max_V)) Serial.print("nan"); else Serial.print(vcell_max_V, 4);
  Serial.print(",");
  Serial.println(max_ok_u8);
}
