# Plan wdrożenia — top_off_water_doser-iot

Projekt łączy dwa systemy w jednym firmware na Seeed XIAO ESP32-C3:
- **Top-off (ATO)** — automatyczne dolewanie wody z detekcją czujnika pływakowego
- **Kalkwasser** — dozowanie wapna (CaCO₃) pompą perystaltyczną + pompą mieszającą

---

## 1. Sprzęt — Seeed Studio XIAO ESP32-C3

### Schemat pinów (widok od góry)

```
                    ┌─────────────────────┐
                    │  Seeed XIAO ESP32-C3│
               5V ──┤ 5V             3.3V ├── 3.3V
              GND ──┤ GND             GND ├── GND
       [D0] GPIO2 ──┤ D0             D10  ├── GPIO10  [D10]
       [D1] GPIO3 ──┤ D1              D9  ├── GPIO9   [D9]
       [D2] GPIO4 ──┤ D2              D8  ├── GPIO8   [D8]
       [D3] GPIO5 ──┤ D3              D7  ├── GPIO20  [D7]
  [D4/SDA] GPIO6 ──┤ D4/SDA          D6  ├── GPIO21  [D6]
  [D5/SCL] GPIO7 ──┤ D5/SCL          5V  ├── (z USB)
                    └─────────────────────┘
                         [USB-C]
```

### Przypisanie pinów (aktualne)

| GPIO | Oznaczenie | Stała | Funkcja | Uwagi |
|------|-----------|-------|---------|-------|
| GPIO 3  | D1  | `MIXING_PUMP_RELAY_PIN`      | Pompa mieszająca kalkwasser | Active HIGH, relay |
| GPIO 4  | D2  | `RESERVE_PUMP_RELAY_PIN`     | Rezerwa (wolna)             | Active HIGH |
| GPIO 5  | D3  | `RESET_PIN`                  | Przycisk resetu / provisioning | INPUT_PULLUP, hold 5s → provisioning |
| GPIO 6  | D4/SDA | `I2C_SDA_PIN`             | Szyna I2C                   | DS3231 RTC + FRAM MB85RC256V |
| GPIO 7  | D5/SCL | `I2C_SCL_PIN`             | Szyna I2C                   | |
| GPIO 8  | D8  | `ERROR_SIGNAL_PIN`           | Sygnalizacja błędów         | HIGH = aktywny, buzzer/LED |
| GPIO 9  | D9  | `WATER_SENSOR_PIN`           | Czujnik pływakowy ATO       | INPUT_PULLUP, active LOW |
| GPIO 10 | D10 | `PERYSTALTIC_PUMP_DRIVER_PIN`| Pompa perystaltyczna kalkwasser | TMC2209 STEP via LEDC |
| GPIO 21 | D6  | `ATO_PUMP_RELAY_PIN`         | Pompa top-off ATO           | **Active LOW** (LOW = ON) |

> **GPIO 2 zwolniony** — był poprzednim ATO_PUMP_RELAY_PIN; ADC1_CH2 koliduje z WiFi
> przez periman (ESP32-C3) → digitalWrite fail podczas transmisji WiFi. Przeniesiono na GPIO21.

> **Czujnik równoległy:** Czujnik pływakowy na jednym pinie GPIO 9.
> Logika: LOW = woda poniżej poziomu, HIGH = poziom OK.

### Topologia I2C

```
GPIO6 (SDA) ──┬──────────────┐
GPIO7 (SCL) ──┤              │
              │              │
         DS3231M RTC    FRAM MB85RC256V
          0x68            0x50
```

---

## 2. Algorytm top-off (ATO)

### 2.1 Maszyna stanów

```
STATE_IDLE
  → (czujnik LOW × DEBOUNCE_COUNTER) →
STATE_DEBOUNCING [obsługiwany przez water_sensors]
  → onDebounceComplete() →
STATE_PUMPING (stała dawka dose_ml, czas = dose_ml / flow_rate)
  → (czas upłynął) →
STATE_LOGGING (5s — zapis do FRAM, obliczenie EMA)
  → STATE_IDLE

STATE_PUMPING → (przekroczono daily_limit_ml) → STATE_ERROR
STATE_PUMPING → (dev_rate_sigma > rate_red_sigma) → STATE_ERROR
STATE_* → handleDirectPumpOn() → STATE_MANUAL_OVERRIDE
```

### 2.2 Algorytm stałej dawki

Pompa pracuje przez **stały czas** wynikający z konfiguracji:

```
dose_time_s = dose_ml / flow_rate_ml_s
```

Parametry przechowywane w FRAM (`TopOffConfig`):
- `dose_ml` — stała dawka na jeden cykl [ml], domyślnie 150 ml
- `daily_limit_ml` — limit bezpieczeństwa na 24h [ml], domyślnie 2000 ml
- `ema_alpha` — współczynnik EMA [0.10–0.40], domyślnie 0.20
- `flow_rate_ml_s` — kalibracja pompy [ml/s] (przechowywana osobno)

### 2.3 Debouncing czujnika

- `DEBOUNCE_INTERVAL_MS = 200` — okres próbkowania
- `DEBOUNCE_COUNTER = 5` — wymagane kolejne odczyty LOW
- Efektywny czas: 200 ms × 5 = **1000 ms**

### 2.4 Struktura rekordu dolewki — `TopOffRecord` (20 bajtów)

```cpp
struct TopOffRecord {
    uint32_t timestamp;      // Unix UTC z DS3231
    uint32_t interval_s;     // Czas od poprzedniej dolewki [s]
    float    rate_ml_h;      // Tempo zużycia wody [ml/h]
    uint16_t cycle_num;      // Sekwencyjny numer cyklu
    int8_t   dev_rate_sigma; // Odchylenie tempa w jednostkach sigma
    uint8_t  alert_level;    // 0=OK, 1=żółty, 2=czerwony
    uint8_t  hour_of_day;    // Godzina lokalna (0–23)
    uint8_t  flags;          // FLAG_MANUAL=0x01, FLAG_BOOTSTRAP=0x02,
                             // FLAG_DAILY_LIM=0x04, FLAG_RED_ALERT=0x08
    uint8_t  _pad[2];
};
// sizeof = 20 — static_assert verified
```

### 2.5 Ring buffer historii

| Parametr | Wartość |
|----------|---------|
| `TOPOFF_HISTORY_SIZE` | 60 slotów |
| Rozmiar rekordu | 20 bajtów |
| Rozmiar w FRAM | 60 × 20 B = 1200 B |
| `history_window_s` | 86400 (24h) domyślnie |

Gdy buffer pełny → nadpisywany najstarszy rekord.

### 2.6 EMA — metryki trendów

Trzy niezależne EMA persystowane w FRAM (`EmaBlock`, 16 bajtów):
- `ema_rate_ml_h` — typowe tempo zużycia wody
- `ema_interval_s` — typowy czas między dolewkami
- `ema_dev_ml_h` — EMA absolutnych odchyleń (podstawa progów dynamicznych)

**Bootstrap:** Pierwsze `DEFAULT_MIN_BOOTSTRAP = 3` cykle nie generują alertów.
Rekordy z okresu bootstrap mają ustawiony flag `FLAG_BOOTSTRAP`.

---

## 3. Kalkwasser — dozowanie

### 3.1 Klasa `KalkwasserScheduler`

Plik: `src/algorithm/kalkwasser_scheduler.h/.cpp`
Instancja globalna: `kalkwasserScheduler`

### 3.2 Harmonogram mieszania (pompa mieszająca)

4× dziennie, o **00:15, 06:15, 12:15, 18:15** (czas lokalny):
- Czas mieszania: `KALK_MIX_DURATION_S = 300 s (5 min)`
- Czas osiadania po mieszaniu: `KALK_SETTLE_S = 3600 s (1h)`
- Mieszanie jest pomijane jeśli trwa cykl ATO (max czekanie: 10 min)

### 3.3 Harmonogram dozowania (pompa perystaltyczna)

Co pełną godzinę za wyjątkiem zablokowanych okien:
- **Dozowanie aktywne:** godziny 02–05, 08–11, 14–17, 20–23 → **16 dawek/dobę**
- Dozowanie jest pomijane gdy trwa cykl ATO lub mieszanie (1h po mix)

Czas pojedynczego dozowania obliczany z:
```
dose_duration_s = (daily_dose_ml × 1000 / KALK_DOSES_PER_DAY) / flow_rate_ul_per_s
```

### 3.4 Stany `KalkwasserState`

```
KALK_IDLE
  ↓ isMixTime && !mixDoneThisSlot
KALK_WAIT_TOPOFF_MIX → (top-off gotowy) → KALK_MIXING (5 min)
  → KALK_SETTLING (60 min) → KALK_IDLE

KALK_IDLE
  ↓ isDoseTime && !doseAlreadyDone
KALK_WAIT_TOPOFF_DOSE → (top-off gotowy) → KALK_DOSING
  → KALK_IDLE

KALK_IDLE → startCalibration() → KALK_CALIBRATING (30s) → KALK_IDLE
```

### 3.5 Kalibracja

Pompa perystaltyczna pracuje przez `KALK_CALIBRATION_S = 30 s`. Użytkownik mierzy
wydaną objętość i wprowadza `measured_ml_30s` przez GUI. System przelicza:
```
flow_rate_ul_per_s = measured_ml_30s × 1000 / 30
```

### 3.6 Alarm „brak top-off"

Jeśli kalkwasser jest aktywny ale `rolling_24h_volume == 0` → alarm `noTopoffAlarm = true`.
Wyświetlany w GUI, reset po wykryciu pierwszego cyklu ATO.

---

## 4. FRAM — layout pamięci (rzeczywiste adresy)

```
0x0000 – 0x0003   Magic number (4 B)
0x0004 – 0x0005   Version (2 B)

0x0018 – 0x0417   Credentials zaszyfrowane AES-256 (1024 B)

0x0500 – 0x0505   [reserved / align]
0x0506 – 0x0509   Volume per second — float (4 B)
0x050A – 0x050B   Volume checksum (2 B)
0x050C – 0x050D   gap1_fail_sum (2 B)
0x050E – 0x050F   gap2_fail_sum (2 B)
0x0510 – 0x0511   water_fail_sum (2 B)
0x0512 – 0x0515   last_reset timestamp (4 B)
0x0516 – 0x0517   stats checksum (2 B)

0x0560 – 0x0573   TopOffConfig struct (20 B)    ← sizeof = 20, magic = 0xA7
0x0574 – 0x0575   TopOffConfig checksum (2 B)

0x0580 – 0x058F   EmaBlock struct (16 B)        ← sizeof = 16
0x0590 – 0x0591   EmaBlock checksum (2 B)

0x0600 – 0x0601   TopOff record count (2 B)
0x0602 – 0x0603   TopOff write pointer (2 B)
0x0610 – 0x0AC0   TopOff ring buffer: 60 × 20 B = 1200 B

0x0AC0 – 0x0AD3   KalkwasserConfig struct (20 B) ← magic = 0xCA
0x0AD4 – 0x0AD5   KalkwasserConfig checksum (2 B)
```

> Pełna lista stałych → `src/hardware/fram_controller.h`

---

## 5. Konfiguracja — Provisioning + GUI

### Zasada: defaults w kodzie, nadpisania w FRAM

```
Przy starcie systemu:
  1. Odczytaj TopOffConfig z FRAM
  2. Jeśli is_configured == 0xA7 (TOPOFF_CONFIG_MAGIC) → użyj wartości z FRAM
  3. Jeśli nie → użyj wartości domyślnych z algorithm_config.h
```

### Pre-fill w provisioning

| Pole | Źródło wartości |
|------|----------------|
| WiFi SSID | FRAM jeśli istnieje, pusty jeśli nie |
| WiFi hasło | **nigdy** — puste = bez zmian |
| Hasło admina | **nigdy** — puste = bez zmian |
| Nazwa urządzenia | FRAM jeśli istnieje, pusty jeśli nie |
| `dose_ml`, `daily_limit_ml` | FRAM jeśli istnieje, domyślna z kodu jeśli nie |
| `ema_alpha` | FRAM jeśli istnieje, domyślna z kodu jeśli nie |
| Progi alertów | FRAM jeśli istnieje, domyślne z kodu jeśli nie |

### Domyślne wartości (`algorithm_config.h`)

```cpp
#define DEFAULT_DOSE_ML             150     // stała dawka [ml]
#define DEFAULT_DAILY_LIMIT_ML     2000     // limit dobowy [ml]
#define DEFAULT_EMA_ALPHA          0.20f    // zakres: 0.10–0.40
#define DEFAULT_RATE_YELLOW_SIGMA   150     // próg żółty (1.5× EMA_dev)
#define DEFAULT_RATE_RED_SIGMA      250     // próg czerwony (2.5× EMA_dev)
#define DEFAULT_HISTORY_WINDOW_S  86400     // okno historii = 24h
#define DEFAULT_MIN_BOOTSTRAP         3     // min dolewek przed alertami
```

---

## 6. API endpoints (zaimplementowane)

### Autentykacja i sesja

```
POST /api/login              → zaloguj (cookie session_token)
POST /api/logout             → wyloguj
GET  /api/health             → status (bez sesji)
```

### System

```
GET  /api/status             → pełny stan systemu JSON
POST /api/system-toggle      → włącz/wyłącz system (+ auto-re-enable 30 min)
POST /api/system-reset       → reset maszyny stanów ATO
```

### ATO — pompa i historia

```
POST /api/pump/direct-on     → ręczne włączenie pompy (bypass algorytmu)
POST /api/pump/direct-off    → ręczne wyłączenie
POST /api/pump/stop          → zatrzymanie awaryjne
GET  /api/pump-settings      → konfiguracja pompy (flow_rate, volume_per_second)
POST /api/pump-settings      → zapis konfiguracji pompy
GET  /api/cycle-history      → ring buffer TopOffRecord (60 rekordów, newest-first)
POST /api/clear-cycle-history→ wyczyść ring buffer
GET  /api/get-statistics     → statystyki błędów
POST /api/reset-statistics   → zeruj statystyki
```

### Objętości i limity

```
GET  /api/daily-volume       → rolling 24h stats
POST /api/reset-daily-volume → zeruj licznik dobowy
GET  /api/available-volume   → dostępna objętość zbiornika
POST /api/set-available-volume   → ustaw dostępną objętość
POST /api/refill-available-volume→ uzupełnij zbiornik do max
GET  /api/fill-water-max     → pobierz daily_limit_ml i dose_ml
POST /api/set-fill-water-max → ustaw daily_limit_ml
POST /api/set-dose           → ustaw dose_ml
```

### Kalkwasser

```
GET  /api/kalkwasser-config     → pobierz konfigurację kalkwasser
POST /api/kalkwasser-config     → zapisz (enabled, daily_dose_ml)
POST /api/kalkwasser-calibrate  → start/stop kalibracji perystaltyki
POST /api/kalkwasser-flow-rate  → zapisz zmierzony przepływ (measured_ml)
```

---

## 7. Zależności

```ini
; platformio.ini
lib_deps =
    ESPAsyncWebServer (me-no-dev)
    AsyncTCP (me-no-dev)
    ArduinoJson ^7.4.2
    RTClib ^2.1.1 (Adafruit)
    Adafruit FRAM I2C
```

---

## 8. Znane problemy i rozwiązania

| Problem | Rozwiązanie |
|---------|-------------|
| GPIO2 koliduje z WiFi (periman) | Przeniesiono ATO relay na GPIO21 (active LOW) |
| PROV_FEEDBACK_PIN = 2 → aktywacja pompy przy boot | Zmieniono na GPIO8 (ERROR_SIGNAL_PIN) |
| Wire.begin() musi być przed initFRAM() | Kolejność w main.cpp: Wire.begin() → initNVS() |
| ESPAsyncWebServer POST: `hasParam(name, true)` wymaga `application/x-www-form-urlencoded` | Wszystkie POST wysyłają form-encoded, nie JSON |








Zapisano. Plik project_service_mode.md zawiera:  