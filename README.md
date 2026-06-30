# BMS SoC Project

Projeto de estimação de Estado de Carga (SoC) para uma célula LG 18650HG2, combinando análise offline de dados, modelos de machine learning e validação experimental num protótipo ESP32.

## Conteúdo
- `notebooks/` — notebooks de treino/validação
- `firmware/` — firmwares

  

## Notebooks Kaggle

- Baselines Random Forest e MLP: [baseline-models-rf-ann.ipynb](./baseline-models-rf-ann-1%20(1).ipynb)
- Modelos leves e seleção da DT depth 6: [lightweight-models.ipynb](./lightweight-models%20(1).ipynb)
- Validação final — 10 Ω, 15 Ω e 22 Ω: [validacao-ensaio-dt.ipynb](./validacao-ensaio-dt.ipynb)
- Validação preliminar do firmware e das estimativas de SoC — Ensaio 1: [valida-o-notebooks-soc (1).ipynb](./valida-o-notebooks-soc%20(1).ipynb)
- Análise exploratória de capacidade para referência de 2,6 Ah: [validacao-soc-2-6-ah.ipynb](./validacao-soc-2-6-ah.ipynb)


  
## Firmwares
- Firmware final — DT depth 6, LUT, contagem de coulombs e CSV: [Firmware_ESP32_DT_pkl_depth6_operacional.ino](./Firmware_ESP32_DT_pkl_depth6_operacional.ino)

- Firmware preliminar — Ensaio 1, LUT, contagem de coulombs e MAX17048: [Firmware_ESP32_Ensaio1_OCV_MAX17048.ino](./Firmware_ESP32_Ensaio1_OCV_MAX17048.ino)



## Dados experimentais

### Ensaios finais

- Ensaio final — carga de 10 Ω: [log_024.csv](./log_024.csv)
- Ensaio final — carga de 15 Ω: [log_026.csv](./log_026.csv)
- Ensaio final — carga de 22 Ω: [log_025.csv](./log_025.csv)

### Ensaio preliminar

- Ensaio 1 — validação inicial de aquisição e estimativas de SoC: [log_001.csv](./log_001.csv)

  
## Nota
O firmware Firmware_ESP32_DT_pkl_depth6_operacional.ino foi usado nos ensaios finais com cargas de 10 Ω, 15 Ω e 22 Ω. O firmware do Ensaio 1 é mantido apenas como registo da fase preliminar de aquisição e não inclui a Árvore de Decisão final.
