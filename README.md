# Micro Gallery

Esse repositório contém os códigos fontes para uma aplicação IoT que carrega dados da API do Cleveland Museum of Art em um dispositivo NodeMCU.
A solução é dividida em três partes: 
- Dispositivo NodeMCU
- Aplicativo Web em Python
- Serviço de Broker MQTT

Para Montagem do dispositivo são necessários os seguintes componentes:
- NodeMCU
- LCD 1.8’ TFT ST7735
- Sensor de vibração SW 420
- Carregar o arquivo iot_project.ino

Para o aplicativo Web, é necessário ter Python 3.8 instalado, baixar esse repositório contendo os arquivos app.py e requirements.txt e seguir os seguintes comandos para iniciar o serviço:
```
$ pip install -r requirements
$ flask run --no-reload --host=0.0.0.0 --port=80
```
