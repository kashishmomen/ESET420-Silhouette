import network
import urequests
import time

#config wifi
SSID = "YOUR_WIFI_NAME" #wifi name
PASSWORD = "YOUR_WIFI_PASSWORD"

#replace ip
SERVER_URL = "http://10.216.5.245:5000/hit"  


def connect_wifi(): 
    wlan = network.WLAN(network.STA_IF) #esp32 connects to wifi
    wlan.active(True)   #turn wifi on

    if not wlan.isconnected():  
        print("Connecting to WiFi...")
        wlan.connect(SSID, PASSWORD)
        while not wlan.isconnected():
            time.sleep(0.5)

    print("WiFi connected:", wlan.ifconfig())

connect_wifi()

#data transmission
def send_hit_data(zone, adc_value, health):
    payload = {
        "zone": zone,
        "adc": adc_value,
        "health": health,
        "timestamp": time.time()
    }

    try:
        r = urequests.post(SERVER_URL, json=payload) #send HTTP post
        r.close()  
        print("Sent:", payload) 
    except Exception as e:  
        print("Transmission failed:", e)

