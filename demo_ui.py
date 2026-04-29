import requests
from datetime import datetime, timezone
from zoneinfo import ZoneInfo
from kivy.network.urlrequest import UrlRequest

from kivy.app import App
from kivy.uix.screenmanager import ScreenManager, Screen
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.scrollview import ScrollView
from kivy.clock import Clock
from kivy.core.window import Window
from kivy.uix.anchorlayout import AnchorLayout
from kivy.uix.floatlayout import FloatLayout
from kivy.uix.image import Image
from kivy.uix.widget import Widget
from kivy.graphics import Color, Ellipse, Rectangle
from kivy.metrics import dp
from kivy.properties import NumericProperty

APP_TZ = ZoneInfo("America/Chicago")

Window.size = (1000, 600)
Window.clearcolor = (0.07, 0.07, 0.07, 1)

SERVER_BASE = "http://192.168.4.1"
REFRESH_RATE = 1.0
MAX_HITS = 7
MAX_HEALTH = 100

LOGO_PATH = "Silhouette_Logo.png"
TARGET_IMAGE_PATH = "CADmodel.png"

ZONE_TO_POS = {
    "HEAD":  (0.51, 0.91),
    "HEART": (0.52, 0.62),
    "CHEST": (0.43, 0.67),
    "LEFT ARM":  (0.27, 0.60),
    "RIGHT ARM": (0.74, 0.60),
    "LEFT LEG":  (0.43, 0.15),
    "RIGHT LEG": (0.58, 0.15),
    "HIPS":  (0.505, 0.40),
}

def damage_for_zone(zone: str) -> int:
    z = (zone or "").upper()
    if "ARM" in z:
        return 15
    if "LEG" in z:
        return 25
    if "HEAD" in z:
        return 100
    if "HEART" in z:
        return 100
    if "CHEST" in z:
        return 55
    if "HIPS" in z:
        return 35
    return 0

def hit_datetime_from_ts(ts):
    if not isinstance(ts, (int, float)):
        return None
    if ts > 1e12:
        ts /= 1000.0
    return datetime.fromtimestamp(ts, tz=timezone.utc).astimezone(APP_TZ)

# ---------- SPLASH SCREEN ----------
class SplashScreen(Screen):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        layout = AnchorLayout()
        logo = Image(
            source=LOGO_PATH,
            allow_stretch=True,
            keep_ratio=True,
            size_hint=(0.7, 0.6),
            size=(600, 100)
        )
        layout.add_widget(logo)
        self.add_widget(layout)

    def on_enter(self):
        Clock.schedule_once(self.go_to_dashboard, 5)

    def go_to_dashboard(self, dt):
        self.manager.current = "dashboard"

# ---------- HIT MARKER ----------
class HitMarker(Widget):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.dot_size = 38
        with self.canvas:
            Color(1, 0, 0, 1)
            self.dot = Ellipse(pos=(0, 0), size=(self.dot_size, self.dot_size))
        self.hide()
        self.disabled = True  # ignore clicks

    def hide(self):
        self.dot.pos = (-9999, -9999)

    def set_center(self, cx, cy):
        r = self.dot_size / 2
        self.dot.pos = (cx - r, cy - r)

class ColoredHealthBar(Widget):
    value = NumericProperty(100)
    max_value = NumericProperty(100)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.size_hint_y = None
        self.height = 45

        with self.canvas:
            self.bg_color = Color(0.2, 0.2, 0.2, 1)
            self.bg_rect = Rectangle(pos=self.pos, size=self.size)

            self.fill_color = Color(0.2, 0.8, 0.2, 1)
            self.fill_rect = Rectangle(pos=self.pos, size=(self.width, self.height))

        self.hp_text = Label(
            text="",
            size_hint=(None, None),
            font_size=38,
            bold=True,
            color=(1, 1, 1, 1)
        )
        self.hp_text.opacity = 0
        self.add_widget(self.hp_text)

        self.bind(pos=self.update_bar,
                  size=self.update_bar,
                  value=self.update_bar,
                  max_value=self.update_bar)

    def update_bar(self, *args):
        self.bg_rect.pos = self.pos
        self.bg_rect.size = self.size

        maxv = max(1.0, float(self.max_value))
        val = float(self.value)
        percent = max(0.0, min(1.0, val / maxv))

        self.fill_rect.pos = self.pos
        self.fill_rect.size = (self.width * percent, self.height)

        if val > 50:
            self.fill_color.rgb = (0.2, 0.8, 0.2)
        elif val > 25:
            self.fill_color.rgb = (1, 0.8, 0.2)
        else:
            self.fill_color.rgb = (1, 0.2, 0.2)

        if 0 < val < maxv:
            self.hp_text.opacity = 1
            self.hp_text.text = str(int(val))

            self.hp_text.texture_update()
            lw, lh = self.hp_text.texture_size
            self.hp_text.size = (lw, lh)

            end_x = self.x + self.width * percent
            x = end_x + dp(8)
            x = min(x, self.x + self.width - lw - dp(4))
            x = max(x, self.x + dp(4))

            y = self.y + (self.height - lh) / 2
            self.hp_text.pos = (x, y)
        else:
            self.hp_text.opacity = 0
            self.hp_text.text = ""

# ---------- DASHBOARD ----------
class DashboardScreen(Screen):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        self._poll_event = None
        self._time_event = None
        self._request_in_flight = False
        self._status_in_flight = False

        self._ts_offset = None
        self._offset_locked = False
        self._fallback_dt_by_index = {}

        self.health = MAX_HEALTH

        root = BoxLayout(orientation="vertical", padding=15, spacing=10)

        # HEADER
        header = FloatLayout(size_hint_y=None, height=150)
        with header.canvas.before:
            Color(0.12, 0.12, 0.12, 1)
            header_bg = Rectangle(pos=header.pos, size=header.size)
        header.bind(pos=lambda inst, val: setattr(header_bg, 'pos', val))
        header.bind(size=lambda inst, val: setattr(header_bg, 'size', val))

        logo = Image(
            source=LOGO_PATH,
            allow_stretch=True,
            keep_ratio=True,
            size_hint=(None, None),
            size=(300, 120),
            pos_hint={"x": 0.02, "center_y": 0.5}
        )
        header.add_widget(logo)

        title = Label(
            text="[b]TrueShot Systems[/b]\nTraining Dashboard",
            markup=True,
            font_size=34,
            halign="center",
            valign="middle",
            size_hint=(1, None),
            height=120,
            pos_hint={"center_x": 0.5, "center_y": 0.5}
        )
        title.bind(size=title.setter("text_size"))
        header.add_widget(title)

        root.add_widget(header)

        # MAIN AREA
        main = BoxLayout(orientation="horizontal", spacing=25)

        # LEFT PANEL
        left_panel = FloatLayout(size_hint_x=0.55)
        self.target_img = Image(
            source=TARGET_IMAGE_PATH,
            allow_stretch=True,
            keep_ratio=True,
            size_hint=(0.9, 0.95),
            pos_hint={"center_x": 0.5, "center_y": 0.5}
        )
        left_panel.add_widget(self.target_img)

        self.marker = HitMarker()
        left_panel.add_widget(self.marker)

        main.add_widget(left_panel)

        # RIGHT PANEL
        right_panel = BoxLayout(orientation="vertical", spacing=15, size_hint_x=0.45)

        today = datetime.now(APP_TZ).strftime("%B %d, %Y")
        right_panel.add_widget(Label(text=f"Date: {today}", size_hint_y=None, height=30))

        self.time_label = Label(text="Current Time: --:--:--", size_hint_y=None, height=30)
        right_panel.add_widget(self.time_label)

        right_panel.add_widget(Label(text="Health", size_hint_y=None, height=20))

        health_row = BoxLayout(size_hint_y=None, height=55, spacing=10)
        health_row.add_widget(Label(text="0", size_hint_x=0.08))

        self.health_bar = ColoredHealthBar()
        self.health_bar.max_value = MAX_HEALTH
        self.health_bar.value = self.health
        health_row.add_widget(self.health_bar)

        health_row.add_widget(Label(text=str(MAX_HEALTH), size_hint_x=0.08))
        right_panel.add_widget(health_row)

        # STATUS
        self.status_label = Label(text="Target Status: Standing", size_hint_y=None, height=40)
        right_panel.add_widget(self.status_label)

        # LOG
        self.log_label = Label(text="", size_hint_y=None)
        self.log_label.bind(texture_size=self.log_label.setter("size"))

        scroll = ScrollView()
        scroll.add_widget(self.log_label)
        right_panel.add_widget(scroll)

        # BUTTONS
        btn_row = BoxLayout(size_hint_y=None, height=60, spacing=20)

        refresh_btn = Button(text="Refresh Log", background_normal="", background_color=(0.2, 0.6, 0.8, 1))
        refresh_btn.bind(on_press=self.load_log)

        reset_btn = Button(text="Reset Session", background_normal="", background_color=(0.8, 0.3, 0.3, 1))
        reset_btn.bind(on_press=self.reset_round)

        btn_row.add_widget(refresh_btn)
        btn_row.add_widget(reset_btn)
        right_panel.add_widget(btn_row)

        main.add_widget(right_panel)
        root.add_widget(main)
        self.add_widget(root)

    # ---------- TIME ----------
    def update_time(self, dt):
        now = datetime.now(APP_TZ).strftime("%I:%M:%S %p").lstrip("0")
        self.time_label.text = f"Current Time: {now}"

    def on_enter(self, *args):
        self.load_log()
        self.load_status()
        self.update_time(0)

        if self._time_event is not None:
            self._time_event.cancel()
        self._time_event = Clock.schedule_interval(self.update_time, 1)

        if self._poll_event is None:
            self._poll_event = Clock.schedule_interval(lambda dt: self.load_log(), REFRESH_RATE)

    def on_leave(self, *args):
        if self._poll_event is not None:
            self._poll_event.cancel()
            self._poll_event = None

        if self._time_event is not None:
            self._time_event.cancel()
            self._time_event = None

    # ---------- STATUS ----------
    def set_status(self, text):
        t = (text or "").upper()

        if "NEUTRAL" in t:
            color = "[color=ff3333]"
        elif "KNOCKED" in t:
            color = "[color=ffcc00]"
        else:
            color = "[color=33ff66]"

        self.status_label.markup = True
        self.status_label.text = f"{color}Target Status: {text}[/color]"

    def load_status(self):
        if self._status_in_flight:
            return
        self._status_in_flight = True

        url = f"{SERVER_BASE}/status"

        def _done(*_):
            self._status_in_flight = False

        def on_success(req, result):
            try:
                dead = bool(result.get("dead_locked", False))
                kd = bool(result.get("knocked_down", False))

                if dead:
                    self.set_status("NEUTRALIZED")
                elif kd:
                    self.set_status("KNOCKED DOWN")
                else:
                    self.set_status("Standing")
            finally:
                _done()

        def on_failure(req, result):
            _done()

        def on_error(req, error):
            _done()

        UrlRequest(url, on_success=on_success, on_failure=on_failure, on_error=on_error, timeout=2, decode=True)

    # ---------- LOGIC ----------
    def load_log(self, *args):
        if self._request_in_flight:
            return
        self._request_in_flight = True

        url = f"{SERVER_BASE}/log"

        def _done(*_):
            self._request_in_flight = False

        def on_success(req, result):
            try:
                hits = result or []

                if not hits:
                    self.log_label.text = "No hits recorded."
                    self.marker.hide()
                    return

                if (not self._offset_locked) and hits:
                    last_ts = hits[-1].get("timestamp", None)
                    last_dt = hit_datetime_from_ts(last_ts)
                    if last_dt is not None:
                        now_dt = datetime.now(APP_TZ)
                        offset = now_dt - last_dt
                        self._ts_offset = offset if abs(offset.total_seconds()) > 120 else None
                    self._offset_locked = True

                display_lines = []
                self.health = MAX_HEALTH
                self.health_bar.max_value = MAX_HEALTH
                self.health_bar.value = self.health

                hit_count = 0
                last_zone = None

                for hit in hits:
                    zone = str(hit.get("zone", "UNKNOWN")).upper()
                    ts = hit.get("timestamp", None)
                    dt = hit_datetime_from_ts(ts)

                    if dt is None:
                        idx = hit_count
                        if idx not in self._fallback_dt_by_index:
                            self._fallback_dt_by_index[idx] = datetime.now(APP_TZ)
                        dt = self._fallback_dt_by_index[idx]

                    if self._ts_offset is not None:
                        dt = dt + self._ts_offset

                    t = dt.strftime("%I:%M:%S %p").lstrip("0")

                    dmg = damage_for_zone(zone)
                    self.health = max(0, self.health - dmg)
                    self.health_bar.value = self.health

                    hit_count += 1
                    display_lines.append(f"{hit_count}. {zone} — {t} — HP: {self.health}")
                    last_zone = zone

                    if self.health <= 0:
                        break
                    if hit_count >= MAX_HITS:
                        break

                self.log_label.text = "\n".join(display_lines)

                if last_zone:
                    self.place_marker(last_zone)
                else:
                    self.marker.hide()

                # ✅ Use ESP32 truth for status
                self.load_status()

            except Exception:
                self.log_label.text = "Bad server response"
            finally:
                _done()

        def on_failure(req, result):
            self.log_label.text = "Server Offline"
            _done()

        def on_error(req, error):
            self.log_label.text = "Server Offline"
            _done()

        UrlRequest(url, on_success=on_success, on_failure=on_failure, on_error=on_error, timeout=2, decode=True)

    def place_marker(self, zone):
        zone = (zone or "").upper()
        if zone not in ZONE_TO_POS:
            self.marker.hide()
            return

        rel_x, rel_y = ZONE_TO_POS[zone]
        img_x, img_y = self.target_img.pos
        img_w, img_h = self.target_img.size

        cx = img_x + rel_x * img_w
        cy = img_y + rel_y * img_h
        self.marker.set_center(cx, cy)

    def reset_round(self, *args):
        try:
            requests.post(f"{SERVER_BASE}/reset", timeout=2)

            self.health = MAX_HEALTH
            self.health_bar.value = self.health
            self.set_status("Standing")
            self.log_label.text = ""
            self.marker.hide()
            self.marker.canvas.ask_update()

            self._ts_offset = None
            self._offset_locked = False
            self._fallback_dt_by_index.clear()

            # refresh server-driven status after reset
            self.load_status()
        except:
            pass

# ---------- APP ----------
class HitLogApp(App):
    def build(self):
        sm = ScreenManager()
        sm.add_widget(SplashScreen(name="splash"))
        sm.add_widget(DashboardScreen(name="dashboard"))
        sm.current = "splash"
        return sm

if __name__ == "__main__":
    HitLogApp().run()