import flask
import requests
import json
import shutil
import random
import dataclasses
from PIL import Image
from enum import Enum
from flask_mqtt import Mqtt

app = flask.Flask(__name__)
app.config["DEBUG"] = False
app.config['MQTT_BROKER_URL'] = '**********'  # use the free broker from HIVEMQ
app.config['MQTT_BROKER_PORT'] = 15686  # default port for non-tls connection
app.config['MQTT_USERNAME'] = '*****'  # set the username here if you need authentication for the broker
app.config['MQTT_PASSWORD'] = '*****'  # set the password here if the broker demands authentication
app.config['MQTT_KEEPALIVE'] = 5  # set the time interval for sending a ping to the broker to 5 seconds
app.config['MQTT_TLS_ENABLED'] = False  # set TLS to disabled for testing purposes

mqtt = Mqtt(app)

@app.route('/', methods=['GET'])
def home():      
    return "HOME"

@app.route('/image', methods=['GET'])
def image():      
    return flask.send_file("img.bmp", mimetype='image/bmp')

def next():
    api_url = "https://openaccess-api.clevelandart.org/api/artworks/"
    headers = {
        'content-type': 'application/json', 
    }
    i = random.randint(1, 1000)
    params = {
        'has_image': '1',
        'skip': i,
        'limit': '1',
    }
    response = requests.get(api_url, params=params, headers=headers)
    obj = response.json()
    title = obj["data"][0]["title"]
    if not 'creators' in obj["data"][0] or len(obj["data"][0]["creators"]) == 0:
        author = ""
    else:
        author = obj["data"][0]["creators"][0]["description"]

    if not 'creation_date' in obj["data"][0]:
        creation_date = ""
    else:
        creation_date = obj["data"][0]["creation_date"]    
    
    tombstone = obj["data"][0]["tombstone"]
    image_url = obj["data"][0]["images"]["web"]["url"]
    file_in = "img.jpg"

    response = requests.get(image_url, stream=True)
    with open(file_in, 'wb') as out_file:
        shutil.copyfileobj(response.raw, out_file)
    del response
       
    img = Image.open(file_in)
    width, height = img.size
    if width > height:
        img = img.rotate(90, expand=True)

    size = 128, 160
    img.thumbnail(size, Image.ANTIALIAS)
    img = img.convert("RGB", palette=Image.ADAPTIVE)

    file_out = "img.bmp"
    img.save(file_out)

    message = {
        "type": "IMAGE",
        "title": title, 
        "author": author, 
        "creation_date": creation_date,
        "url": "http://192.168.0.1:80/image",
    }
    mqtt.publish('ads', json.dumps(message))

@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    mqtt.subscribe('ads')

@mqtt.on_message()
def handle_mqtt_message(client, userdata, message):
    data = dict(
        topic=message.topic,
        payload=message.payload.decode()
    )

    message = json.loads(data["payload"])
    if message["type"] == "COMMAND":
        if message["value"] == "NEXT":
            next()
    
    print(message)

if __name__ == '__main__':
    app.run()
