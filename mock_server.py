from flask import Flask, request, jsonify
from flask_cors import CORS
import time

app = Flask(__name__)
CORS(app)

MAX_HEALTH = 100
health = MAX_HEALTH
knocked_down = False
hit_log = []

damage_table = {
    "HEAD": 100,
    "HEART": 100,

    "CHEST": 55,
    "HIPS": 35,

    "LEFT_ARM": 15,
    "RIGHT_ARM": 15,

    "LEFT_LEG": 25,
    "RIGHT_LEG": 25,
}

@app.get("/status")
def status():
    return jsonify({"health": health, "knocked_down": knocked_down})

@app.get("/log")
def log():
    return jsonify(hit_log)

@app.post("/reset")
def reset():
    global health, knocked_down, hit_log
    health = MAX_HEALTH
    knocked_down = False
    hit_log = []
    return jsonify({"ok": True})

@app.post("/hit")
def hit():
    """
    Send JSON like:
    {"zone":"CHEST","adc":2100}
    """
    global health, knocked_down, hit_log
    data = request.get_json(force=True) or {}
    zone = (data.get("zone") or "CHEST").upper()
    adc = int(data.get("adc") or 2000)

    dmg = damage_table.get(zone, 0)
    if zone in ("HEAD", "HEART"):
        knocked_down = True
        health = 0
    else:
        health = max(0, health - dmg)
        if health == 0:
            knocked_down = True

    entry = {
        "zone": zone,
        "adc": adc,
        "health": health,
        "timestamp": time.time()
    }
    hit_log.append(entry)
    return jsonify(entry)

if __name__ == "__main__":
    # accessible on own computer
    app.run(host="127.0.0.1", port=5050, debug=True)