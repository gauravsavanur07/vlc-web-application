/*
App.py
G Savanur
12/7/18
*/
from random import randint
from threading import Thread
import secrets
from flask import Flask, jsonify, request, abort, make_response
import subprocess
import os
import time

DEBUG_MODE = True
SOCKET_ADDRESS = 'ws://127.0.0.1'

app = Flask(__name__)

@app.route('/api/instance', methods=['POST'])
def create_instance():
  token = secrets.token_hex() # Random secret key
  port = randint(6000, 7000) # Random port

  vlmc_params = ['./vlmc',
                 '/Users/alpercakan/Desktop/space/da/da.vlmc',
                 str(port),
                 token,
                 request.environ.get('REMOTE_ADDR', ''),
                 request.environ.get('HTTP_ORIGIN', '')]
  print(request.environ.get('REMOTE_ADDR', ''))
  instance = subprocess.Popen(vlmc_params)

  content = jsonify(
    {
      'authToken': token,
      'socketAddress': f'{SOCKET_ADDRESS}:{port}'
    }
  )

  resp = make_response(content)
  resp.headers['Access-Control-Allow-Origin'] = '*'
  return resp 

if __name__ == '__main__':
  app.run(debug=DEBUG_MODE)

