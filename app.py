from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
from fastapi.responses import StreamingResponse
import paho.mqtt.client as mqtt
import aiosqlite
import json, asyncio
import threading
import os

FILE_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
DB_FILE = os.path.join(FILE_DIRECTORY, "sensors.db")
IMAGES_DIRECTORY = os.path.join(FILE_DIRECTORY, "immagini")
STATIC_DIRECTORY = os.path.join(FILE_DIRECTORY, "static")

loop = None

app = FastAPI()
clients: list[asyncio.Queue] = []

mqttClient = mqtt.Client()
#mqttClient.connect(host = "192.168.4.1", port = 1887, clean_start = MQTT_CLEAN_START_FIRST_ONLY)
#mqttClient.subscribe("stazione1/sensori", 1)
#mqttClient.subscribe("stazione1/immagini", 2)

##Versione MQTT broker:mosquitto version 2.0.21

@app.get("/last_read")
async def getLastRead():
	async with aiosqlite.connect(DB_FILE) as db:
		db.row_factory = aiosqlite.Row
		cursor = await db.execute("SELECT * FROM dataset ORDER BY timestamp DESC LIMIT 1;")
		result = dict(await cursor.fetchone())
	result.pop("timestamp")
	result.pop("file_path")
	return result

@app.get("/sse")
async def establish_sse():
	q = asyncio.Queue()
	clients.append(q)
	
	async def event_generator():
		try:
			while True:
				try:	
					data = await asyncio.wait_for(q.get(), timeout=30.0)
					yield f"data: {json.dumps(data)}\n\n"
				except asyncio.TimeoutError:
					yield ": Keep Alive Message\n\n"
		except asyncio.CancelledError:
			clients.remove(q)

	return StreamingResponse(event_generator(), media_type="text/event-stream")

app.mount("/", StaticFiles(directory=STATIC_DIRECTORY, html=True), name="static")

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
		image_path = os.path.join(IMAGES_DIRECTORY, f"{data['timestamp']}.jpg")
		async with aiosqlite.connect(DB_FILE) as db:
			await db.execute("INSERT INTO dataset (timestamp, temperatura, pressione, umidita, luce, vento, monossido_carb, qualita_aria, file_path) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);", 
				(
					data["timestamp"], 
					data["temperatura"], 
					data["pressione"], 
					data["umidita"], 
					data["luce"], 
					data["vento"], 
					data["monossido_carb"], 
					data["qualita_aria"], 
					image_path
				)
			)
			await db.commit()
	except aiosqlite.IntegrityError as e:
		return
	
	#broadcast flag to each client
	await broadcast({"event": "new_reading"})


def save_image(timestamp: str, image: bytes):
	image_path = os.path.join(IMAGES_DIRECTORY, f"{timestamp}.jpg")
	with open(image_path, "wb") as f:
		f.write(image)

@app.on_event("startup")
async def fastApiStartup():
	async with aiosqlite.connect(DB_FILE) as db:
		await db.execute("CREATE TABLE IF NOT EXISTS dataset(" \
		"timestamp     TEXT    NOT NULL," \
		"temperatura   DOUBLE  NOT NULL," \
		"pressione     DOUBLE  NOT NULL," \
		"umidita       DOUBLE  NOT NULL," \
		"luce          DOUBLE," \
		"vento         DOUBLE," \
		"monossido_carb DOUBLE," \
		"qualita_aria  DOUBLE," \
		"file_path     TEXT    NOT NULL," \
		"PRIMARY KEY (timestamp));")
		await db.commit()

	global loop
	loop = asyncio.get_event_loop()
	threading.Thread(target=start_mqtt, daemon=True).start()

def start_mqtt():
	mqttClient.on_message = on_message
	mqttClient.connect(host = "192.168.4.1", port = 1887, clean_start = mqtt.MQTT_CLEAN_START_FIRST_ONLY)
	mqttClient.subscribe("stazione1/sensori", 1)
	mqttClient.subscribe("stazione1/immagini", 2)
	mqttClient.loop_forever()




		
		

		

