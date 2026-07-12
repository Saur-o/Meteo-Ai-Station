import numpy as np
import pandas as pd
import os
import json
from flask import Flask, request, jsonify
from tensorflow.keras import models 
from sklearn.preprocessing import MinMaxScaler

REPO = os.path.dirname(os.path.abspath(__file__))
MODEL_PATH = os.path.join(REPO, "lstm.keras")

features = ['temperatura', 'pressione', 'umidita', 'luce']

DF1 = pd.read_csv(os.path.join(REPO, "df1.csv"))
DF2 = pd.read_csv(os.path.join(REPO, "df2.csv"))
DF_COMB = pd.concat([DF1[features], DF2[features]])

scaler = MinMaxScaler()
scaler.fit(DF_COMB)

last_request = pd.DataFrame()
last_prevision = {}

app = Flask(__name__)

lstm_model = models.load_model(MODEL_PATH, compile=True) 

#content-type application/json
@app.route("/prediction", methods=["POST"])
def prevision():
	input_window = request.get_json()
	input_window = pd.DataFrame(input_window)
	global last_request
	global last_prevision
	global scaler
	global features
	global lstm_model 

	if (input_window.equals(last_request)):
		return jsonify(data=last_prevision)

	last_request = input_window 
	scaled_request = scaler.transform(last_request[features])
	scaled_request = scaled_request.reshape(1, 24, 4)

	scaled_answer = lstm_model.predict(scaled_request)
	unscaled_answer = scaler.inverse_transform(scaled_answer)

	return_dict = {}

	for i, col in enumerate(features):
		return_dict[col] = float(unscaled_answer[0, i])
	
	last_prevision = return_dict

	return jsonify(data=return_dict)