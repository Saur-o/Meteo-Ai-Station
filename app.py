from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
import paho.mqtt.client as mqtt
import json, asyncio
import os
import threads
from datetime import Datetime
import sqlite3

app = FastAPI()
clients: list[asyncio.Queue] = []

connection = sqlite3.connect("sensors.db")
cursor = conn.cursor()

mqttClient = mqtt.Client()
mqttClient.connect(host = "192.168.4.1", port = 1887, clean_start = MQTT_CLEAN_START_FIRST_ONLY)
mqttClient.subscribe("stazione1/sensori", 1)
mqttClient.subscribe("stazione1/immagini", 2)

##Versione MQTT broker:mosquitto version 2.0.21

app.mount("/", StaticFiles(directory="static", html=True), name="static")

@app.get("/last_read")
async def getLastRead():
	cursor.execute("SELECT * FROM dataset ORDER BY timestamp DESC;")
	result = cursor.fetchone()
	return result

async def broadcast(data: dict):
	for q in clients:
		await q.put(data)

@mqttClient.on_message()
def on_message(client, userdata, message):
	if message.topic == "stazione1/sensori":
		data = json.load(message.payload.decode("utf-8"))
		#riempire campi e dati inseriti quando la convenzione di quello che inviamo e il nome dei campi è assicurato
		try:
			cursor.execute("INSERT INTO dataset () VALUES ();")
		except sqlite3.IntegrityError as e:
			return
		#broadcast flag to each client
	elif message.topic == "stazione1/immagini":
		#vedere di quanti byte è composto il metadata dell'immagine con il timestamp
		data = message.payload
		timestampImage = (data[:8]).decode("utf-8")
		image = data[8:]


		
		

		

