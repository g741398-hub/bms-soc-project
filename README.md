# bms-soc-project
bms-soc-project with 
# BMS SoC Project

Projeto de estimação de SoC para célula LG 18650HG2 com validação experimental em ESP32.

## Conteúdo
- `notebooks/` — notebooks de treino/validação
-

## Notebooks Kaggle
- private/full dataset preprocessing: https://github.com/g741398-hub/bms-soc-project/blob/main/dhakal-full%20(2).ipynb
- baseline RF/ANN: [[link]](https://github.com/g741398-hub/bms-soc-project/blob/main/baseline-models-rf-ann-1%20(2).ipynb)
- lightweight models (DT): [[link]](https://github.com/g741398-hub/bms-soc-project/blob/main/lightweight-models%20(1).ipynb)
- validação firmware: https://github.com/g741398-hub/bms-soc-project/blob/main/valida-o-notebooks-soc%20(1).ipynb
- validação soc: https://github.com/g741398-hub/bms-soc-project/blob/main/validacao-soc-2-6-ah.ipynb

## Nota
No Ensaio 1, o firmware usado calcula SoC por OCV + LUT + Coulomb Counting.
A integração da DT no firmware corresponde a uma fase posterior.
