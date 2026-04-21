from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
import paho.mqtt.client as mqtt
import json, asyncio
import os
import threading
from datetime import Datetime
import aiosqlite

loop = None

app = FastAPI()
clients: list[asyncio.Queue] = []

mqttClient = mqtt.Client()
mqttClient.connect(host = "192.168.4.1", port = 1887, clean_start = MQTT_CLEAN_START_FIRST_ONLY)
mqttClient.subscribe("stazione1/sensori", 1)
mqttClient.subscribe("stazione1/immagini", 2)

##Versione MQTT broker:mosquitto version 2.0.21

app.mount("/", StaticFiles(directory="static", html=True), name="static")

@app.get("/last_read")
async def getLastRead():
	async with aiosqlite.connect("sensor.db") as db:
		cursor = await db.execute("SELECT * FROM dataset ORDER BY timestamp DESC;")
		result = await cursor.fetchone()
	return result

async def broadcast(data: dict):
	for q in clients:
		await q.put(data)

def on_message(client, userdata, message):
	if message.topic == "stazione1/sensori":
		data = json.loads(message.payload.decode("utf-8"))
		asyncio.run_coroutine_threadsafe(insert_data(data), loop)
	elif message.topic == "stazione1/immagini":
		#vedere di quanti byte è composto il metadata dell'immagine con il timestamp
		data = message.payload
		timestampImage = (data[:19]).decode("utf-8")
		image = data[19:]
		threading.Thread(target=save_image, args=(timestampImage, image)).start()
		

async def insert_data(data: dict):
	#riempire campi e dati inseriti quando la convenzione di quello che inviamo e il nome dei campi è assicurato
	try:
		async with aiosqlite.connect("sensor.db") as db:
			await db.execute("INSERT INTO dataset () VALUES ();")
			await db.commit()
	except aiosqlite.IntegrityError as e:
		return
	
	#broadcast flag to each client
	await broadcast({"event": "new_reading"})


def save_image(timestamp: str, image: bytes):
	with open(f"{timestamp}.jpg", "wb") as f:
			f.write(image)

@app.on_event("startup")
async def fastApiStartup():
	global loop
	loop = asyncio.get_event_loop()
	threading.Thread(target=start_mqtt, daemon=True).start()


		
		

		

