from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
from fastapi.responses import StreamingResponse
import paho.mqtt.client as mqtt
import aiosqlite
import json, asyncio
import threading

loop = None

app = FastAPI()
clients: list[asyncio.Queue] = []

mqttClient = mqtt.Client()
#mqttClient.connect(host = "192.168.4.1", port = 1887, clean_start = MQTT_CLEAN_START_FIRST_ONLY)
#mqttClient.subscribe("stazione1/sensori", 1)
#mqttClient.subscribe("stazione1/immagini", 2)

##Versione MQTT broker:mosquitto version 2.0.21

app.mount("/", StaticFiles(directory="static", html=True), name="static")

@app.get("/last_read")
async def getLastRead():
	async with aiosqlite.connect("sensor.db") as db:
		cursor = await db.execute("SELECT * FROM dataset ORDER BY timestamp DESC LIMIT 1;")
		result = await cursor.fetchone()
	return result

@app.get("/sse")
async def establish_sse():
	q = asyncio.queue()
	clients.append(q)
	
	async def event_generator():
		try:
			while True:
				try:	
					data = await asyncio.wait_for(q.get(), timeout=30.0)
					yield f"data: {json.dumps(data)}\n\n"
				except asynctio.TimeOut:
					yield ":"Keep Alive Message\n\n"
		except asyncio.CancelledError:
			clients.remove(q)

	return StreamingResponse(event_generator(), media_type="text/event-stream")

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

def start_mqtt():
	mqttClient.on_message = on_message
	mqttClient.connect(host = "192.168.4.1", port = 1887, clean_start = mqtt.MQTT_CLEAN_START_FIRST_ONLY)
	mqttClient.subscribe("stazione1/sensori", 1)
	mqttClient.subscribe("stazione1/immagini", 2)
	mqttClient.loop_forever()




		
		

		

