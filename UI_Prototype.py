import requests
from datetime import datetime
from kivy.uix.anchorlayout import AnchorLayout
from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.button import Button
from kivy.uix.label import Label
from kivy.uix.scrollview import ScrollView
from kivy.uix.floatlayout import FloatLayout
from kivy.uix.image import Image
from kivy.uix.widget import Widget
from kivy.graphics import Color, Ellipse

server_base = "http://127.0.0.1:5050"  # change to your ip

TARGET_IMAGE_PATH = "Software/target.png"   # <-- update this

ZONE_TO_POS = {
    "HEAD":  (0.50, 0.85),
    "HEART": (0.50, 0.60),
    "CHEST": (0.50, 0.52),
    "ARMS":  (0.20, 0.52),
    "LEGS":  (0.50, 0.28),
    "HIPS":  (0.50, 0.38),
}

def format_time_from_hit(hit: dict) -> str:
    """
    Supports:
      - hit["timestamp"] as epoch seconds (float/int)
      - hit["timestamp"] as ISO string
      - missing timestamp -> uses now
    """
    ts = hit.get("timestamp", None)
    dt = None

    if isinstance(ts, (int, float)):
        dt = datetime.fromtimestamp(ts)
    elif isinstance(ts, str):
        # try ISO format
        try:
            dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
            dt = dt.astimezone()  # local time
        except Exception:
            dt = None

    if dt is None:
        dt = datetime.now()

    return dt.strftime("%I:%M:%S %p").lstrip("0")

class HitMarker(Widget):
    """A simple red dot that we can move around."""
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.dot_size = 18
        with self.canvas:
            Color(1, 0, 0, 1)  # red
            self.dot = Ellipse(pos=(0, 0), size=(self.dot_size, self.dot_size))
        self.hide()

    def hide(self):
        self.dot.pos = (-9999, -9999)

    def set_center(self, cx, cy):
        # Ellipse uses bottom-left, so offset by radius
        r = self.dot_size / 2
        self.dot.pos = (cx - r, cy - r)

class HitLogUI(BoxLayout):
    def __init__(self, **kwargs):
        super().__init__(orientation="vertical", **kwargs)

        self.title = Label(text="Training Round Hit Log", size_hint_y=0.1)
        self.add_widget(self.title)

        today = datetime.now().strftime("%B %d, %Y")
        self.date_label = Label(
            text=f"Date: {today}",
            size_hint_y=None,
            height=40,
            font_size=20
        )       
        self.add_widget(self.date_label)

        # --- Logo ---
        logo_row = AnchorLayout(
            size_hint_y=None,
            height=120,
            anchor_x="center",
            anchor_y="center"
        )

        logo = Image(
            source="Software/Silhouette_Logo.png",
            allow_stretch=True,
            keep_ratio=True,
            size_hint=(None, None),
            size=(260, 90)
        )

        logo_row.add_widget(logo)
        self.add_widget(logo_row)

        self.target_area = FloatLayout(size_hint_y=0.55)

        self.target_img = Image(
            source=TARGET_IMAGE_PATH,
            allow_stretch=True,
            keep_ratio=True,
            size_hint=(1, 1),
            pos_hint={"x": 0, "y": 0}
        )
        self.target_img.bind(pos=self._on_target_change, size=self._on_target_change)
        self.target_area.add_widget(self.target_img)

        self.marker = HitMarker()
        self.target_area.add_widget(self.marker)

        self.add_widget(self.target_area)

        self.log_label = Label(text="", size_hint_y=None)
        self.log_label.bind(texture_size=self.log_label.setter("size"))

        scroll = ScrollView()
        scroll.add_widget(self.log_label)
        self.add_widget(scroll)

        refresh_btn = Button(text="Refresh Log", size_hint_y=0.1)
        refresh_btn.bind(on_press=self.load_log)

        reset_btn = Button(text="Reset Round", size_hint_y=0.1)
        reset_btn.bind(on_press=self.reset_round)

        self.add_widget(refresh_btn)
        self.add_widget(reset_btn)

        self.load_log()

    def _update_log_size(self, *args):
        self.log_label.text_size = (self.log_label.width, None)
        self.log_label.height = self.log_label.texture_size[1]

    def _place_marker_for_zone(self, zone: str):
        zone = (zone or "").upper()
        if zone not in ZONE_TO_POS:
            self.marker.hide()
            return

        rel_x, rel_y = ZONE_TO_POS[zone]

        # Convert relative -> pixel coords within the image widget area
        # Use the target_img's on-screen size/pos
        img_x, img_y = self.target_img.pos
        img_w, img_h = self.target_img.size

        cx = img_x + rel_x * img_w
        cy = img_y + rel_y * img_h
        self.marker.set_center(cx, cy)

    def _on_target_change(self, *args):
            # Reposition marker based on the most recent zone
            if hasattr(self, "last_zone"):
                self._place_marker_for_zone(self.last_zone)

    def load_log(self, *args):
        try:
            r = requests.get(f"{server_base}/log", timeout=2)

            # Show clearer errors instead of JSON decode crash
            if r.status_code != 200:
                self.log_label.text = f"Server error {r.status_code}\n{r.text[:200]}"
                self.marker.hide()
                return

            hits = r.json()
            if not hits:
                self.log_label.text = "No hits recorded."
                self.marker.hide()
                return

            last = hits[-1]
            self.last_zone = last.get("zone", "")
            self._place_marker_for_zone(self.last_zone)

            # Build display text with timestamps
            display_lines = []
            for i, hit in enumerate(hits):
                t = format_time_from_hit(hit)
                zone = hit.get("zone", "UNKNOWN")
                adc = hit.get("adc", "—")
                health = hit.get("health", "—")

                display_lines.append(
                    f"Hit {i+1} — {t}\n"
                    f"  Zone: {zone}\n"
                    f"  ADC: {adc}\n"
                    f"  Health: {health}\n"
                )

            self.log_label.text = "\n".join(display_lines)

            # Move marker to the most recent hit
            last = hits[-1]
            self._place_marker_for_zone(last.get("zone", ""))

        except Exception as e:
            self.log_label.text = f"Connection error:\n{e}"
            self.marker.hide()

    def reset_round(self, *args):
        try:
            r = requests.post(f"{server_base}/reset", timeout=2)
            if r.status_code != 200:
                self.log_label.text = f"Reset failed ({r.status_code})\n{r.text[:200]}"
                return
            self.load_log()
        except Exception as e:
            self.log_label.text = f"Reset failed:\n{e}"

class HitLogApp(App):
    def build(self):
        return HitLogUI()

if __name__ == "__main__":
    HitLogApp().run()
