# BMS SoC Project

Projeto de estimação de Estado de Carga (SoC) para uma célula LG 18650HG2, combinando análise offline de dados, modelos de machine learning e validação experimental num protótipo ESP32.

## Conteúdo
- `notebooks/` — notebooks de treino/validação
- `firmware/` — firmwares

## Notebooks Kaggle
- private/full dataset preprocessing: https://github.com/g741398-hub/bms-soc-project/blob/main/dhakal-full%20(2).ipynb
- baseline RF/ANN: [[link]](https://github.com/g741398-hub/bms-soc-project/blob/main/baseline-models-rf-ann-1%20(2).ipynb)
- lightweight models (DT): [[link]](https://github.com/g741398-hub/bms-soc-project/blob/main/lightweight-models%20(1).ipynb)
- validação firmware: https://github.com/g741398-hub/bms-soc-project/blob/main/valida-o-notebooks-soc%20(1).ipynb
- validação soc: https://github.com/g741398-hub/bms-soc-project/blob/main/valida-o-notebooks-soc%20(1).ipynb
- soh: https://github.com/g741398-hub/bms-soc-project/blob/main/validacao-soc-2-6-ah.ipynb
 -notebook de validação final — ensaios de 10 Ω, 15 Ω e 22 Ω:  https://github.com/g741398-hub/bms-soc-project/blob/main/validacao-ensaio-dt%20(5).ipynb
  
## firmwares
-firmware final: https://github.com/g741398-hub/bms-soc-project/blob/main/Firmware_ESP32_DT_pkl_depth6_operacional.ino

-firmware preliminar : https://github.com/g741398-hub/bms-soc-project/blob/main/Firmware_ESP32_Ensaio1_OCV_MAX17048.ino

## Nota
O firmware Firmware_ESP32_DT_pkl_depth6_operacional.ino foi usado nos ensaios finais com cargas de 10 Ω, 15 Ω e 22 Ω. O firmware do Ensaio 1 é mantido apenas como registo da fase preliminar de aquisição e não inclui a Árvore de Decisão final.
