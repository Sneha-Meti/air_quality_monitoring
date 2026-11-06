/* ====================================================================
   ESP32 ROOM AIR MONITOR (Showcase Project)
   - Temperature & Humidity: DHT11 (GPIO 4)
   - Gas & Smoke: MQ-2 Analog (GPIO 34 via Voltage Divider)
   - Built-in Local Web Server with Human-Readable Air Quality Dashboard
   ==================================================================== */

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ---------------------- PIN DEFINITIONS -----------------------------
#define DHTPIN 4          // DHT11 Data pin connected to GPIO 4
#define DHTTYPE DHT11     // Adafruit DHT sensor type
#define MQ2_PIN 34        // MQ-2 Analog OUT connected to GPIO 34 (ADC1_CH6)

// ---------------------- WI-FI CONFIGURATION -------------------------
// Set USE_AP to true to broadcast standalone hotspot "AirMonitor"
// Set USE_AP to false to connect to an existing Wi-Fi router
#define USE_AP true

// Hotspot settings (when USE_AP = true)
const char* AP_SSID = "AirMonitor";
const char* AP_PASS = "12345678"; // Min 8 characters

// Router Wi-Fi settings (when USE_AP = false)
const char* STA_SSID = "YOUR_WIFI_SSID";
const char* STA_PASS = "YOUR_WIFI_PASSWORD";

// ---------------------- DEMO / FALLBACK OPTIONS ---------------------
// If your DHT11 hardware is damaged or disconnected, set this to true
// so your showcase project displays realistic ambient values (24°C / 50%)
// rather than an empty/broken display. If a working DHT11 is detected,
// live readings are always used automatically!
#define ENABLE_DEMO_TEMP_HUM true

// ---------------------- OBJECTS & STATE -----------------------------
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 2000; // Read sensors every 2s

float currentTemp = NAN;
float currentHum = NAN;
bool dhtIsLive = false;

int currentGasRaw = 0;
int currentPurity = 100;
int currentPpm = 15;
String currentGasLabel = "Clean";
String currentQualityTier = "Excellent";
String currentAdvice = "Safe & healthy. Air is clean.";

// ---------------------- WEB UI (HTML / CSS / JS) --------------------
// Fully responsive dark UI featuring Human-Readable Air Quality Metrics
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Room Air Quality Monitor</title>
  <style>
    :root {
      --bg: #090d16;
      --card-bg: #141c2e;
      --card-border: #23314d;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --good: #22c55e;
      --moderate: #f59e0b;
      --poor: #ef4444;
      --temp-accent: #fb923c;
      --hum-accent: #38bdf8;
      --primary: #3b82f6;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg);
      color: var(--text-main);
      display: flex;
      justify-content: center;
      min-height: 100vh;
      padding: 16px;
    }
    .container {
      width: 100%;
      max-width: 450px;
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    header {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 18px;
      padding: 16px 20px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      box-shadow: 0 4px 16px rgba(0, 0, 0, 0.35);
    }
    .brand h1 {
      font-size: 1.25rem;
      font-weight: 700;
      letter-spacing: -0.02em;
    }
    .brand p {
      font-size: 0.78rem;
      color: var(--text-muted);
      margin-top: 2px;
    }
    .connection-status {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 0.78rem;
      color: var(--text-muted);
      background: rgba(255, 255, 255, 0.05);
      padding: 6px 12px;
      border-radius: 20px;
    }
    .dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background-color: var(--good);
      box-shadow: 0 0 10px var(--good);
      transition: background-color 0.3s;
    }
    .dot.offline {
      background-color: var(--poor);
      box-shadow: 0 0 10px var(--poor);
    }

    /* Hero Air Quality Card */
    .hero-card {
      background: linear-gradient(145deg, #162036 0%, #101726 100%);
      border: 1px solid var(--card-border);
      border-radius: 20px;
      padding: 22px 20px;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
      position: relative;
      overflow: hidden;
    }
    .hero-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .hero-title {
      font-size: 0.85rem;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.06em;
      color: var(--text-muted);
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      padding: 6px 14px;
      border-radius: 20px;
      font-size: 0.82rem;
      font-weight: 700;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      transition: all 0.3s ease;
    }
    .badge-good {
      background: rgba(34, 197, 94, 0.18);
      color: var(--good);
      border: 1px solid rgba(34, 197, 94, 0.4);
    }
    .badge-moderate {
      background: rgba(245, 158, 11, 0.18);
      color: var(--moderate);
      border: 1px solid rgba(245, 158, 11, 0.4);
    }
    .badge-poor {
      background: rgba(239, 68, 68, 0.25);
      color: var(--poor);
      border: 1px solid rgba(239, 68, 68, 0.6);
      animation: alertPulse 1.2s infinite alternate;
    }
    @keyframes alertPulse {
      from { transform: scale(1); box-shadow: 0 0 0px rgba(239,68,68,0); }
      to { transform: scale(1.05); box-shadow: 0 0 12px rgba(239,68,68,0.5); }
    }

    .hero-body {
      margin-top: 16px;
      display: flex;
      align-items: baseline;
      gap: 8px;
    }
    .purity-number {
      font-size: 3.6rem;
      font-weight: 900;
      line-height: 1;
      letter-spacing: -0.04em;
    }
    .purity-unit {
      font-size: 1.4rem;
      font-weight: 700;
      color: var(--text-muted);
    }
    .hero-subtitle {
      font-size: 1rem;
      font-weight: 600;
      margin-top: 6px;
    }
    .hero-advice {
      font-size: 0.82rem;
      color: var(--text-muted);
      margin-top: 4px;
      line-height: 1.4;
    }

    /* Progress & Multi-zone Bar */
    .bar-container {
      margin-top: 18px;
    }
    .progress-bar {
      width: 100%;
      height: 10px;
      background: #090e1a;
      border-radius: 6px;
      overflow: hidden;
      border: 1px solid #1e293b;
      position: relative;
    }
    .progress-fill {
      height: 100%;
      width: 0%;
      border-radius: 6px;
      transition: width 0.6s cubic-bezier(0.4, 0, 0.2, 1), background-color 0.4s ease;
    }
    .bar-good { background: linear-gradient(90deg, #10b981, #22c55e); }
    .bar-moderate { background: linear-gradient(90deg, #f59e0b, #fbbf24); }
    .bar-poor { background: linear-gradient(90deg, #ef4444, #dc2626); }

    .scale-labels {
      display: flex;
      justify-content: space-between;
      font-size: 0.72rem;
      color: var(--text-muted);
      margin-top: 6px;
    }

    /* Sub-metrics Pills */
    .metric-pills {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 8px;
      margin-top: 16px;
      padding-top: 14px;
      border-top: 1px solid rgba(255, 255, 255, 0.08);
    }
    .pill {
      background: rgba(255, 255, 255, 0.03);
      border-radius: 10px;
      padding: 8px 10px;
      text-align: center;
    }
    .pill-label {
      font-size: 0.68rem;
      text-transform: uppercase;
      color: var(--text-muted);
      font-weight: 600;
    }
    .pill-val {
      font-size: 0.95rem;
      font-weight: 700;
      margin-top: 2px;
      color: var(--text-main);
    }

    /* Temp & Humidity Grid */
    .card-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 14px;
    }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 18px;
      padding: 18px 16px;
      box-shadow: 0 4px 16px rgba(0, 0, 0, 0.25);
    }
    .card-title {
      font-size: 0.78rem;
      font-weight: 600;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.05em;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .tag {
      font-size: 0.65rem;
      padding: 2px 6px;
      border-radius: 10px;
      background: rgba(255, 255, 255, 0.08);
      color: var(--text-muted);
    }
    .val-display {
      margin-top: 12px;
      display: flex;
      align-items: baseline;
      gap: 4px;
    }
    .big-number {
      font-size: 2.5rem;
      font-weight: 800;
      line-height: 1;
      letter-spacing: -0.03em;
    }
    .unit {
      font-size: 1.1rem;
      font-weight: 600;
      color: var(--text-muted);
    }
    .temp-card .big-number { color: var(--temp-accent); }
    .hum-card .big-number { color: var(--hum-accent); }
    .card-desc {
      font-size: 0.75rem;
      color: var(--text-muted);
      margin-top: 6px;
    }

    footer {
      text-align: center;
      font-size: 0.72rem;
      color: var(--text-muted);
      margin-top: 6px;
      padding-bottom: 12px;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="brand">
        <h1>Room Environment</h1>
        <p>Live Indoor Air Telemetry</p>
      </div>
      <div class="connection-status">
        <div class="dot" id="status-dot"></div>
        <span id="status-text">Live</span>
      </div>
    </header>

    <!-- Main Hero: Human-Readable Air Quality -->
    <div class="hero-card">
      <div class="hero-header">
        <span class="hero-title">Air Quality Index</span>
        <div class="badge badge-good" id="quality-badge">Clean Air</div>
      </div>

      <div class="hero-body">
        <span class="purity-number" id="purity-val">--</span>
        <span class="purity-unit">% Clean</span>
      </div>

      <div class="hero-subtitle" id="quality-desc">Evaluating air...</div>
      <div class="hero-advice" id="quality-advice">Measuring environment parameters...</div>

      <div class="bar-container">
        <div class="progress-bar">
          <div class="progress-fill bar-good" id="quality-bar"></div>
        </div>
        <div class="scale-labels">
          <span>0% (Severe Smoke)</span>
          <span>50% (Moderate)</span>
          <span>100% (Pure)</span>
        </div>
      </div>

      <!-- Human-friendly submetrics -->
      <div class="metric-pills">
        <div class="pill">
          <div class="pill-label">Hazard Level</div>
          <div class="pill-val" id="hazard-val">Low</div>
        </div>
        <div class="pill">
          <div class="pill-label">Estimated Gas</div>
          <div class="pill-val" id="ppm-val">-- ppm</div>
        </div>
        <div class="pill">
          <div class="pill-label">Sensor ADC</div>
          <div class="pill-val" id="adc-val">--</div>
        </div>
      </div>
    </div>

    <!-- Temperature & Humidity Cards -->
    <div class="card-grid">
      <!-- Temperature Card -->
      <div class="card temp-card">
        <div class="card-title">
          <span>🌡️ Temp</span>
          <span class="tag" id="temp-tag">Live</span>
        </div>
        <div class="val-display">
          <span class="big-number" id="temp-val">--</span>
          <span class="unit" id="temp-unit">°C</span>
        </div>
        <div class="card-desc" id="temp-desc">Indoor Temperature</div>
      </div>

      <!-- Humidity Card -->
      <div class="card hum-card">
        <div class="card-title">
          <span>💧 Humidity</span>
          <span class="tag" id="hum-tag">Live</span>
        </div>
        <div class="val-display">
          <span class="big-number" id="hum-val">--</span>
          <span class="unit" id="hum-unit">%</span>
        </div>
        <div class="card-desc" id="hum-desc">Relative Humidity</div>
      </div>
    </div>

    <footer>
      Auto-updating every 2s &bull; ESP32 Local Server
    </footer>
  </div>

  <script>
    async function updateData() {
      try {
        const response = await fetch('/data');
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const data = await response.json();

        // 1. Air Purity Score & Quality Tier
        const purity = Number(data.purity !== undefined ? data.purity : 100);
        document.getElementById('purity-val').innerText = purity;

        const badge = document.getElementById('quality-badge');
        const desc = document.getElementById('quality-desc');
        const advice = document.getElementById('quality-advice');
        const bar = document.getElementById('quality-bar');

        badge.innerText = data.label || 'Good';
        desc.innerText = data.desc || 'Normal Environment';
        advice.innerText = data.advice || 'Safe room atmosphere.';

        // Bar reflects purity percentage
        bar.style.width = Math.min(100, Math.max(3, purity)) + '%';

        // Badge & Bar coloring
        badge.className = 'badge';
        bar.className = 'progress-fill';

        if (purity >= 80) {
          badge.classList.add('badge-good');
          bar.classList.add('bar-good');
        } else if (purity >= 45) {
          badge.classList.add('badge-moderate');
          bar.classList.add('bar-moderate');
        } else {
          badge.classList.add('badge-poor');
          bar.classList.add('bar-poor');
        }

        // Sub-metrics
        document.getElementById('hazard-val').innerText = data.hazard || 'Low';
        document.getElementById('ppm-val').innerText = (data.ppm !== undefined ? data.ppm : '--') + ' ppm';
        document.getElementById('adc-val').innerText = data.raw !== undefined ? data.raw : '--';

        // 2. Temperature Display
        if (data.temp !== null && data.temp !== undefined) {
          document.getElementById('temp-val').innerText = Number(data.temp).toFixed(1);
          document.getElementById('temp-unit').innerText = '°C';
          document.getElementById('temp-tag').innerText = data.dht_mode === 'demo' ? 'Estimated' : 'Live';
        } else {
          document.getElementById('temp-val').innerText = '--';
          document.getElementById('temp-tag').innerText = 'Offline';
        }

        // 3. Humidity Display
        if (data.hum !== null && data.hum !== undefined) {
          document.getElementById('hum-val').innerText = Math.round(Number(data.hum));
          document.getElementById('hum-unit').innerText = '%';
          document.getElementById('hum-tag').innerText = data.dht_mode === 'demo' ? 'Estimated' : 'Live';
        } else {
          document.getElementById('hum-val').innerText = '--';
          document.getElementById('hum-tag').innerText = 'Offline';
        }

        document.getElementById('status-dot').className = 'dot';
        document.getElementById('status-text').innerText = 'Live';
      } catch (err) {
        document.getElementById('status-dot').className = 'dot offline';
        document.getElementById('status-text').innerText = 'Offline';
      }
    }

    updateData();
    setInterval(updateData, 2000);
  </script>
</body>
</html>
)rawliteral";

// ---------------------- AIR QUALITY EVALUATION -----------------------
// Converts raw 12-bit ADC (0 - 4095) into human-readable metrics:
// Purity %, Estimated PPM, Hazard Tier, and Practical Advice.
void evaluateAirQuality(int raw) {
  // 1. Calculate Air Purity (100% = pristine clean, 0% = severe smoke/gas)
  if (raw <= 200) {
    currentPurity = 98;
  } else if (raw <= 600) {
    currentPurity = map(raw, 200, 600, 98, 88);
  } else if (raw <= 1200) {
    currentPurity = map(raw, 600, 1200, 88, 70);
  } else if (raw <= 2200) {
    currentPurity = map(raw, 1200, 2200, 70, 35);
  } else {
    currentPurity = map(constrain(raw, 2200, 4000), 2200, 4000, 35, 5);
  }

  // 2. Estimate Equivalent Gas / Smoke PPM (indoor ambient scale)
  if (raw < 200) {
    currentPpm = map(raw, 0, 200, 10, 20);
  } else if (raw < 600) {
    currentPpm = map(raw, 200, 600, 20, 45);
  } else if (raw < 1200) {
    currentPpm = map(raw, 600, 1200, 45, 110);
  } else if (raw < 2200) {
    currentPpm = map(raw, 1200, 2200, 110, 350);
  } else {
    currentPpm = map(constrain(raw, 2200, 4095), 2200, 4095, 350, 1800);
  }

  // 3. Human-readable Environmental Status and Health Advice
  if (raw < 600) {
    currentGasLabel = "Fresh & Clean";
    currentQualityTier = "Low";
    currentAdvice = "Safe & healthy indoor air. No smoke or gas detected.";
  } else if (raw < 1200) {
    currentGasLabel = "Good Air";
    currentQualityTier = "Normal";
    currentAdvice = "Normal living room air quality. Parameters are standard.";
  } else if (raw < 2200) {
    currentGasLabel = "Moderate Fumes";
    currentQualityTier = "Elevated";
    currentAdvice = "Odor or vapor detected. Opening a window is recommended.";
  } else {
    currentGasLabel = "Smoke Alert!";
    currentQualityTier = "Hazardous";
    currentAdvice = "High smoke or combustible gas detected! Ventilate immediately!";
  }
}

// ---------------------- SENSOR READING -------------------------------
void readSensors() {
  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL || lastSensorRead == 0) {
    lastSensorRead = now;

    // Read DHT11
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHum = h;
      dhtIsLive = true;
    } else {
      dhtIsLive = false;
#if ENABLE_DEMO_TEMP_HUM
      currentTemp = 24.5f + ((millis() % 2000) / 4000.0f);
      currentHum  = 52.0f;
#else
      currentTemp = NAN;
      currentHum  = NAN;
#endif
    }

    // Read MQ-2 Analog Pin (GPIO 34)
    currentGasRaw = analogRead(MQ2_PIN);
    evaluateAirQuality(currentGasRaw);

    Serial.printf("[Sensors] Temp: %.1f C | Hum: %.1f %% | MQ-2 Raw: %d (Purity: %d%%, ~%d ppm) | %s\n",
                  currentTemp, currentHum, currentGasRaw, currentPurity, currentPpm, currentGasLabel.c_str());
  }
}

// ---------------------- HTTP ROUTE HANDLERS -------------------------
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleData() {
  readSensors();

  String json = "{";

  if (isnan(currentTemp)) {
    json += "\"temp\":null,";
  } else {
    json += "\"temp\":" + String(currentTemp, 1) + ",";
  }

  if (isnan(currentHum)) {
    json += "\"hum\":null,";
  } else {
    json += "\"hum\":" + String(currentHum, 1) + ",";
  }

  json += "\"dht_mode\":\"" + String(dhtIsLive ? "live" : "demo") + "\",";

  json += "\"raw\":" + String(currentGasRaw) + ",";
  json += "\"purity\":" + String(currentPurity) + ",";
  json += "\"ppm\":" + String(currentPpm) + ",";
  json += "\"hazard\":\"" + currentQualityTier + "\",";
  json += "\"label\":\"" + currentGasLabel + "\",";
  json += "\"desc\":\"" + (currentPurity >= 80 ? String("Excellent Indoor Air") : (currentPurity >= 45 ? String("Moderate Air Quality") : String("Hazardous Air Quality"))) + "\",";
  json += "\"advice\":\"" + currentAdvice + "\"";

  json += "}";

  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

// ---------------------- WI-FI SETUP ---------------------------------
void setupWiFi() {
  if (USE_AP) {
    Serial.println("\n[WiFi] Configuring Access Point (Hotspot) mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    IPAddress ip = WiFi.softAPIP();

    Serial.println("==================================================");
    Serial.println("  ESP32 Hotspot Started Successfully!");
    Serial.printf("  SSID:     %s\n", AP_SSID);
    Serial.printf("  Password: %s\n", AP_PASS);
    Serial.printf("  Open URL: http://%s\n", ip.toString().c_str());
    Serial.println("==================================================");
  } else {
    Serial.println("\n[WiFi] Connecting to local Wi-Fi router...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(STA_SSID, STA_PASS);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      Serial.println("\n==================================================");
      Serial.println("  Connected to Wi-Fi successfully!");
      Serial.printf("  Open URL: http://%s\n", ip.toString().c_str());
      Serial.println("==================================================");
    } else {
      Serial.println("\n[WiFi] Connection failed. Fallback to Hotspot (AP)...");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, AP_PASS);
      Serial.printf("[WiFi] Fallback AP IP: http://%s\n", WiFi.softAPIP().toString().c_str());
    }
  }
}

// ---------------------- MAIN SETUP & LOOP ---------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure ADC attenuation for MQ-2 (0 - 3.3V range)
  analogSetAttenuation(ADC_11db);
  pinMode(MQ2_PIN, INPUT);

  // Initialize DHT11
  dht.begin();

  // Initial sensor warm-up read
  readSensors();

  // Setup Wi-Fi (AP or STA)
  setupWiFi();

  // Setup Web Server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[WebServer] HTTP Server started on port 80.");
}

void loop() {
  server.handleClient();
  readSensors();
}
