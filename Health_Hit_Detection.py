from machine import Pin, ADC, PWM
import time

servo
servo_pin = 18
servo_freq = 50
knockdown_angle = 45
reset_angle = 90

#servo = PWM(Pin(servo_pin), freq=servo_freq)

def set_servo_angle(angle):
    duty = int(26 + (angle / 180) * 102)
    servo.duty(duty)

set_servo_angle(reset_angle)


knockdown_angle = 45
reset_angle = 90

#hit detection logic
hit_threshold = 1800     # ADC threshold
lockout_ms = 50          # prevent piezo ringing
last_hit_time = 0

#health logic
total_health = 100

damage_table = {
    "CHEST": 55,
    "ARMS": 15,
    "LEGS": 25,
    "HIPS": 35,
    "LEFT_ARM": 15,
    "RIGHT_ARM": 15,
    "LEFT_LEG": 25,
    "RIGHT_LEG": 25
}

#zone pin
zones = {
    "HEAD":  ADC(Pin(36)),
    "HEART": ADC(Pin(39)),
    "CHEST": ADC(Pin(34)),
    "LEFT_ARM":   ADC(Pin(35)),
    "RIGHT_ARM":  ADC(Pin(32)),
    "LEFT_LEG":   ADC(Pin(33)),
    "RIGHT_LEG":  ADC(Pin(25)),
    "HIPS":  ADC(Pin(33))
}

#adc range config
for adc in zones.values():
    adc.atten(ADC.ATTN_11DB)

#hit detection
def hit_detected(adc):
    global last_hit_time

    adc_value = adc.read()
    current_time = time.ticks_ms()

    if (adc_value >= hit_threshold and
        time.ticks_diff(current_time, last_hit_time) > lockout_ms):
        last_hit_time = current_time
        return True, adc_value

    return False, adc_value

#health/zone
def process_hit(zone):
    global total_health

    print("Hit detected in zone:", zone)

    #fatal zones
    if zone == "HEAD" or zone == "HEART":
        print("FATAL HIT → KNOCKDOWN")
        set_servo_angle(knockdown_angle)
        return

    # non-fatal zones
    zone_damage = damage_table.get(zone, 0)
    total_health -= zone_damage

    print("Damage:", zone_damage)
    print("Remaining health:", total_health)

    if total_health <= 0:
        print("Health depleted → KNOCKDOWN")
        set_servo_angle(knockdown_angle)

#main
print("System ready. Monitoring piezo sensors...")

while True:
    for zone_name, adc in zones.items():
        hit, value = hit_detected(adc)

        if hit:
            print("ADC value:", value)
            process_hit(zone_name)
            time.sleep_ms(100)  # small delay after hit
            break

    time.sleep_ms(5)
