from backend import *
from fabric.api import env
import json


### rest
from flask import Flask, request
from flask_restful import Resource, Api

app = Flask(__name__)
api = Api(app)

def add_cors_header(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Methods'] = 'HEAD, GET, POST, PATCH, PUT, OPTIONS, DELETE'
    response.headers['Access-Control-Allow-Headers'] = 'Origin, X-Requested-With, Content-Type, Accept'
    response.headers['Access-Control-Allow-Credentials'] = 'true'
    return response

env.password = 'admin'

routers = [
        {
        'id' : 1,
        'props':
            {
                'message' : 'none',
                'users' : '',
                'wifiusers' : '',
                'current_strategy' : 'best-route',
            },
        },
        {
        'id' : 2,
        'props':
            {
                'message' : 'none',
                'users' : '',
                'wifiusers' : '',
                'current_strategy' : 'best-route',
            }
        }
        ]

class RoutersStart(Resource):
    def get(self):
        #todo check return
        try:
            start_ndn()
            set_broadcast(routers)
            return routers
        except:
            return 'ERROR'

class Strategy_Best(Resource):
    def get(self):
        #todo check return
        try:
            set_best_route(routers)
            return routers
        except:
            return 'ERROR'

class Strategy_Broadcast(Resource):
    def get(self):
        #todo check return
        try:
            set_broadcast(routers)
            return routers
        except:
            return 'ERROR'

class Stats(Resource):
    def get(self):
        #todo check return
        try:
            get_users(routers)
            return routers
        except:
            return 'ERROR'

class WiFiUsers(Resource):
    def get(self):
        #todo check return
        try:
            getWifiUers(routers)
            return routers
        except:
            return 'ERROR'



api.add_resource(RoutersStart, '/routers/start')
api.add_resource(Strategy_Best, '/routers/strategy_best')
api.add_resource(Strategy_Broadcast, '/routers/strategy_broadcast')
api.add_resource(Stats, '/routers/users')
api.add_resource(WiFiUsers, '/routers/wifiusers')
app.after_request(add_cors_header)


if __name__ == '__main__':
    app.run(debug=True)
