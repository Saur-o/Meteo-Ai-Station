from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
import paho.mqtt.client as mqtt
import json
import os
import threads
from datetime import Datetime
import sqlite3

app = FastApi()

connection = sqlite.connect("sensors.db")
cursor = conn.cursor()

clients = []

##Versione MQTT broker:mosquitto version 2.0.21 starting

app.mount("/", StaticFiles(directory="static/home.html"), name="html")

@app.get("/last_read")
async def getLastRead():
	cursor.execute(SELECT * FROM dataset ORDER BY timestamp DESC);
	result = cursor.fetchone()
	return result



def broadcast():
