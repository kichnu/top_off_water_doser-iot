# Plan wdrożenia — top_off_water_doser-iot

Projekt łączy dwa systemy w jednym firmware na Seeed XIAO ESP32-C3:
- **Top-off** — dolewanie wody z detekcją czujnika pływakowego (para równoległa)
- **Doser** — dozowanie nawozów dwoma niezależnymi pompami (2 kanały)

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

### Przypisanie pinów

| GPIO | Oznaczenie | Funkcja | Uwagi |
|------|-----------|---------|-------|
| GPIO 2  | D0       | `ERROR_SIGNAL_PIN` — sygnalizacja błędu | Active HIGH, buzzer/LED |
| GPIO 3  | D1       | `WATER_SENSOR_PIN` — czujnik pływakowy (para równoległa) | INPUT_PULLUP, active LOW |
| GPIO 4  | D2       | **WOLNY** — GPIO 4 zwolniony (drugi czujnik wyeliminowany) | rezerwa |
| GPIO 5  | D3       | `DOSE_RELAY_PIN_CH1` — pompa dozująca A | Active HIGH, relay OUT |
| GPIO 6  | D4/SDA   | `I2C_SDA_PIN` — szyna I2C | DS3231 RTC + FRAM MB85RC256V |
| GPIO 7  | D5/SCL   | `I2C_SCL_PIN` — szyna I2C | |
| GPIO 8  | D8/BOOT  | `RESET_PIN` — przycisk resetu/provisioning | INPUT_PULLUP, hold 5s → provisioning |
| GPIO 9  | D9/MISO  | `DOSE_RELAY_PIN_CH2` — pompa dozująca B | Active HIGH, relay OUT |
| GPIO 10 | D10/MOSI | `PUMP_RELAY_PIN` — pompa top-off | Active HIGH, relay OUT |
| GPIO 20 | D7/RX    | **WOLNY** (rezerwa) | Unikać — UART debug |
| GPIO 21 | D6/TX    | **WOLNY** (rezerwa) | Unikać — UART debug |

> **Czujniki równolegle:** Dwa czujniki pływakowe na tym samym poziomie połączone równolegle
> do jednego pinu GPIO 3. Logika: LOW = woda poniżej poziomu (którykolwiek czujnik reaguje).
> Redundancja sprzętowa — awaria jednego czujnika nie blokuje systemu.

### Topologia I2C

```
GPIO6 (SDA) ──┬──────────────┐
GPIO7 (SCL) ──┤              │
              │              │
         DS3231M RTC    FRAM MB85RC256V
          0x68            0x50
```

---

## 2. Algorytm top-off — zmiany

### 2.1 Debouncing czujnika (uproszczony)

**Usunięte** z obecnego kodu: `PRE_QUALIFICATION`, `SETTLING_TIME`, `TOTAL_DEBOUNCE_TIME`.

**Pozostaje:**
- `DEBOUNCE_INTERVAL_MS` — okres jednego próbkowania (np. 200 ms)
- `DEBOUNCE_COUNTER` — liczba kolejnych zgodnych odczytów wymagana do zmiany stanu (np. 5)
- Efektywny czas debounce = `DEBOUNCE_INTERVAL_MS × DEBOUNCE_COUNTER`

**RELEASE VERIFICATION** — pozostaje bez zmian, dotyczy jednego pinu (GPIO 3).

**Logika debounce:**
```
Każde DEBOUNCE_INTERVAL_MS:
  odczytaj GPIO 3
  jeśli odczyt == poprzedni:
    counter++
  else:
    counter = 0  ← reset przy niestabilnym sygnale
  jeśli counter >= DEBOUNCE_COUNTER:
    zmień stan czujnika → wyzwól akcję
```

### 2.2 Dynamiczne dozowanie (Wariant B + timeout)

Pompa nie pracuje przez stały czas — pracuje **aż czujnik wróci do HIGH**.

```
FLOW:
  1. Czujnik → stabilnie LOW (po debounce)
  2. Uruchom pompę
  3. Czekaj na czujnik HIGH  ←── warunek stopu
     LUB upłynął MAX_DOSE_TIME_S  ←── bezpiecznik
  4. Zatrzymaj pompę
  5. Oblicz faktyczną objętość: volume_ml = duration_s × flow_rate_ml_s
  6. Zapisz TopOffRecord do FRAM
  7. Zaktualizuj EMA i metryki
  8. Jeśli timeout → ustaw flagę TIMEOUT_HIT → alarm
```

**Parametry (konfigurowane przez użytkownika):**

| Parametr | Rola | Zakres | Domyślna |
|----------|------|--------|---------|
| `max_dose_ml` | Górny limit bezpieczeństwa jednej dolewki; gdy przekroczony → timeout + alarm | 50–2000 ml | 300 ml |
| `flow_rate_ml_s` | Wydajność pompy [ml/s] z kalibracji — do obliczenia objętości | kalibracja w GUI | — |

`MAX_DOSE_TIME_S` wyliczany z: `max_dose_ml / flow_rate_ml_s` (nie hardkodowany).

### 2.3 Struktura rekordu dolewki — `TopOffRecord`

Rozmiar: **18 bajtów / rekord**.

```cpp
struct TopOffRecord {
    uint32_t timestamp;       // Unix UTC z DS3231 RTC
    uint16_t volume_ml;       // Faktycznie dolana objętość = duration_s × flow_rate_ml_s
    uint32_t interval_s;      // Czas od poprzedniej dolewki [s]
    float    rate_ml_h;       // Tempo zużycia wody [ml/h] = volume_ml / interval_s × 3600
    int8_t   dev_volume_pct;  // Odchylenie obj. od EMA_volume [%], zakres -127..+127
    int8_t   dev_rate_pct;    // Odchylenie rate od EMA_rate [%]
    uint8_t  alert_level;     // 0 = OK, 1 = ŻÓŁTY, 2 = CZERWONY
    uint8_t  flags;           // Patrz definicja flag poniżej
};

// flags bitmask:
// bit 0 — MANUAL       (dolewka wyzwolona ręcznie z GUI)
// bit 1 — TIMEOUT_HIT  (pompa zatrzymana przez timeout, czujnik nie zwolnił)
// bit 2 — BOOTSTRAP    (rekord z okresu budowania historii EMA, < MIN_BOOTSTRAP_COUNT)
```

### 2.4 Ring buffer — okno czasowe 24–48h

Buffer **nie ma stałej liczby slotów z rotacją** — ma stałą pulę slotów, ale aktywne są tylko
te z `timestamp > now - HISTORY_WINDOW_S`.

| Parametr | Wartość |
|----------|---------|
| `HISTORY_WINDOW_S` | 86400 (24h) — docelowo konfigurowalne 86400/129600/172800 |
| `TOPOFF_HISTORY_SIZE` | 60 slotów (zapas na 48h przy 30 dolewkach/dobę) |
| Rozmiar w FRAM | 60 × 18 B = **1080 bajtów** |

Gdy buffer pełny → nadpisywany najstarszy rekord (klasyczny ring buffer z write pointer).
Przy obliczaniu metryk pomijane rekordy starsze niż `HISTORY_WINDOW_S`.

### 2.5 Metryki trendów — EMA + odchylenia

Obliczane **po każdej dolewce**, wynik cache'owany w RAM i persystowany w FRAM (blok EMA).

#### EMA — Exponential Moving Average

```
EMA_nowa = α × wartość_bieżąca + (1 - α) × EMA_poprzednia
```

Trzy niezależne EMA (float, persystowane w FRAM):
- `ema_volume_ml` — typowa objętość dolewki
- `ema_interval_s` — typowy czas między dolewkami
- `ema_rate_ml_h` — typowe tempo zużycia wody [ml/h]

**Współczynnik α** — konfigurowany przez użytkownika, zakres **0.10–0.40**, domyślnie 0.20.

Interpretacja α:
- 0.10 → wolna adaptacja, wygładza krótkotrwałe skoki (norma stabilna)
- 0.20 → balans — reaguje po ~5 zdarzeniach
- 0.40 → szybka adaptacja (zmienne warunki, częste zmiany trybu)

**Bootstrap:** Pierwsze `MIN_BOOTSTRAP_COUNT` (domyślnie 5) zdarzeń budują EMA bez alertów.
GUI informuje: "Budowanie historii: 3/5". Rekordy z tego okresu mają flagę `BOOTSTRAP`.

#### Odchylenia procentowe

```
dev_volume_pct = (volume_ml - ema_volume_ml) / ema_volume_ml × 100
dev_rate_pct   = (rate_ml_h - ema_rate_ml_h) / ema_rate_ml_h × 100
```

#### Poziomy alertów — dwa niezależne wymiary

Progi konfigurowane przez użytkownika (provisioning + GUI):

| Wskaźnik | ZIELONY | ŻÓŁTY | CZERWONY |
|----------|---------|-------|---------|
| `dev_volume_pct` | < `vol_yellow_pct` | `vol_yellow_pct` – `vol_red_pct` | > `vol_red_pct` |
| `dev_rate_pct` | < `rate_yellow_pct` | `rate_yellow_pct` – `rate_red_pct` | > `rate_red_pct` |

Domyślne wartości progów:

| Parametr | Domyślna | Zakres konfiguracji |
|----------|---------|-------------------|
| `vol_yellow_pct` | 30% | 10–80% |
| `vol_red_pct` | 60% | 20–150% |
| `rate_yellow_pct` | 40% | 10–100% |
| `rate_red_pct` | 80% | 20–200% |

Końcowy `alert_level = max(alert_volume, alert_rate)`.
Niezależnie: `TIMEOUT_HIT` zawsze = poziom CZERWONY.

#### Blok EMA w FRAM — persystencja przez restart

```
EmaBlock (17 bajtów):
  ema_volume_ml    float    4B
  ema_interval_s   float    4B
  ema_rate_ml_h    float    4B
  bootstrap_count  uint8_t  1B   ← liczba zebranych zdarzeń (do MIN_BOOTSTRAP_COUNT)
  _padding         uint8_t  4B
```

---

## 3. Konfiguracja parametrów — Provisioning + GUI

### Zasada: defaults w kodzie, nadpisania w FRAM

```
Przy starcie systemu:
  1. Odczytaj blok konfiguracji z FRAM
  2. Jeśli blok ważny (magic OK) → użyj wartości z FRAM
  3. Jeśli blok pusty/uszkodzony → użyj wartości domyślnych z algorithm_config.h
```

Domyślne wartości są zawsze w kodzie (`algorithm_config.h`). FRAM przechowuje tylko
to, co użytkownik świadomie zmienił. Aktualizacja firmware może dostarczyć nowe
wartości domyślne bez potrzeby migracji FRAM.

### Pre-fill w provisioning

Formularz captive portal zawsze pokazuje **aktualne wartości działające w systemie**
(FRAM jeśli istnieje, kod jeśli nie):

| Pole | Źródło wartości w input | Uwaga |
|------|------------------------|-------|
| WiFi SSID | FRAM jeśli istnieje, pusty jeśli nie | |
| WiFi hasło | **nigdy** — zawsze puste | Puste = zostaw bez zmian |
| Hasło admina | **nigdy** — zawsze puste | Puste = zostaw bez zmian |
| Nazwa urządzenia | FRAM jeśli istnieje, pusty jeśli nie | |
| `max_dose_ml` | FRAM jeśli istnieje, **domyślna z kodu** jeśli nie | |
| `alpha` | FRAM jeśli istnieje, **domyślna z kodu** jeśli nie | |
| Progi alertów | FRAM jeśli istnieje, **domyślne z kodu** jeśli nie | |
| `HISTORY_WINDOW_S` | FRAM jeśli istnieje, **domyślna z kodu** jeśli nie | |

Przy pierwszym uruchomieniu (puste FRAM) użytkownik widzi gotowe, sensowne wartości
domyślne — może je zaakceptować bez zmian lub dostosować.

### Domyślne wartości w kodzie (`algorithm_config.h`)

```cpp
// Debouncing
#define DEFAULT_DEBOUNCE_INTERVAL_MS     200
#define DEFAULT_DEBOUNCE_COUNTER         5       // efektywnie 1000 ms

// Dozowanie dynamiczne
#define DEFAULT_MAX_DOSE_ML              300     // limit bezpieczeństwa [ml]

// EMA
#define DEFAULT_EMA_ALPHA                0.20f   // zakres użytkownika: 0.10–0.40

// Progi alertów [%]
#define DEFAULT_VOL_YELLOW_PCT           30
#define DEFAULT_VOL_RED_PCT              60
#define DEFAULT_RATE_YELLOW_PCT          40
#define DEFAULT_RATE_RED_PCT             80

// Okno historii
#define DEFAULT_HISTORY_WINDOW_S         86400   // 24h; opcje: 86400/129600/172800

// Bootstrap
#define DEFAULT_MIN_BOOTSTRAP_COUNT      5
```

### Dwupoziomowy dostęp do parametrów

| Parametr | Provisioning | GUI (dashboard) |
|----------|:---:|:---:|
| WiFi SSID | ✓ | — |
| WiFi hasło | ✓ | — |
| Hasło admina | ✓ | — |
| Nazwa urządzenia | ✓ | — |
| `max_dose_ml` | ✓ | ✓ |
| `alpha` (EMA) | ✓ | ✓ |
| `vol_yellow_pct` / `vol_red_pct` | ✓ | ✓ |
| `rate_yellow_pct` / `rate_red_pct` | ✓ | ✓ |
| `HISTORY_WINDOW_S` | ✓ | ✓ |
| Kalibracja pompy top-off (`flow_rate_ml_s`) | — | ✓ tylko |
| Konfiguracja kanałów dosera | — | ✓ tylko |

### API endpoints — konfiguracja algorytmu

```
GET  /api/topoff/settings    → zwraca bieżącą konfigurację (max_dose, alpha, progi, okno)
POST /api/topoff/settings    → zapisuje konfigurację do FRAM
GET  /api/topoff/history     → ring buffer rekordów + metryki EMA + daily_buckets
GET  /api/topoff/status      → bieżący stan: czujnik, pompa, alert_level, ema values
POST /api/topoff/calibrate   → start/stop kalibracji pompy (flow_rate_ml_s)
POST /api/topoff/pump/manual → ręczna dolewka (wyzwolenie cyklu)
POST /api/topoff/pump/stop   → natychmiastowe zatrzymanie pompy
```

### Odpowiedź GET /api/topoff/history

```json
{
  "records": [
    {
      "ts": 1709500000,
      "vol": 87,
      "interval_s": 14400,
      "rate_ml_h": 21.75,
      "dev_vol": 12,
      "dev_rate": 8,
      "alert": 0,
      "flags": 0
    }
  ],
  "ema": {
    "volume_ml": 82.4,
    "interval_s": 15200.0,
    "rate_ml_h": 19.5,
    "bootstrap_count": 5,
    "is_ready": true
  },
  "daily_buckets": [185, 210, 195, 175, 220, 87, 0],
  "alert_level": 0,
  "window_hours": 24
}
```

`daily_buckets` — suma ml per dzień dla ostatnich 7 dni (obliczana z ring buffera przy każdym
wywołaniu). Przeglądarka rysuje bar chart (Chart.js) bez żadnych obliczeń po stronie ESP32.

---

## 4. Nowa funkcjonalność — Dosing System (2 kanały)

### 4.1 Źródło

Logika i GUI przeniesione z projektu `dosing_system-iot`, okrojone do **2 kanałów**:
- `CHANNEL_COUNT = 2`
- Kanał A → GPIO 5 (`DOSE_RELAY_PIN_CH1`)
- Kanał B → GPIO 9 (`DOSE_RELAY_PIN_CH2`)

### 4.2 Funkcje

| Funkcja | Opis |
|---------|------|
| Harmonogram | Per kanał: bitmaska dni tygodnia + godzina |
| Dozowanie manualne | Wyzwolenie z GUI, kanał + objętość |
| Limity dobowe | Per kanał, reset RTC-based o 00:00 UTC |
| Kalibracja pompy | Czas pracy → flow_rate [ml/s] per kanał |
| Śledzenie pojemnika | Suma dozowań vs pojemność pojemnika |

### 4.3 API endpoints

| Metoda | Ścieżka | Opis |
|--------|---------|------|
| GET | `/api/dose/status` | Stan kanałów (harmonogram, ostatnia dawka, pojemnik) |
| POST | `/api/dose/manual` | Ręczne wyzwolenie kanału |
| GET | `/api/dose/settings` | Konfiguracja kanałów |
| POST | `/api/dose/settings` | Zapis konfiguracji |
| POST | `/api/dose/calibrate` | Start/stop kalibracji pompy kanału |

---

## 5. FRAM — layout pamięci

```
0x0000 – 0x0017   Magic number + wersja firmware (24 B)
0x0018 – 0x0417   Credentials zaszyfrowane AES-256 (1024 B)

0x0500 – 0x051F   Top-off: konfiguracja algorytmu (32 B)
                    max_dose_ml, flow_rate_ml_s, alpha,
                    vol_yellow_pct, vol_red_pct,
                    rate_yellow_pct, rate_red_pct,
                    history_window_s
0x0520 – 0x0530   Top-off: blok EMA (17 B)
                    ema_volume_ml, ema_interval_s, ema_rate_ml_h,
                    bootstrap_count
0x0531 – 0x055F   Top-off: statystyki błędów i liczniki (47 B)
                    total_topoffs, total_timeouts, last_topoff_ts,
                    rolling_24h_write_ptr
0x0600 – 0x0857   Top-off: ring buffer 60 × TopOffRecord (18 B)  → 1080 B

0x1000 – 0x10FF   Doser: konfiguracja kanałów CH1 + CH2 (256 B)
0x1100 – 0x113F   Doser: liczniki dobowe i tygodniowe (64 B)
0x1140 – 0x1FFF   Doser: ring buffer rekordów dozowań
```

> Szczegółowe adresy i struktury C++ → `src/hardware/fram_constants.h` (do aktualizacji).

---

## 6. Kolejność implementacji

1. **`hardware_pins.h`** — zaktualizować: jeden `WATER_SENSOR_PIN`, dodać `DOSE_RELAY_PIN_CH1/CH2`
2. **`algorithm_config.h`** — nowe parametry: debounce, max_dose, alpha, progi alertów
3. **`fram_constants.h`** — nowy pełny layout z sekcjami top-off config/EMA/ring-buffer i doser
4. **Algorytm top-off** — nowy debouncing + dynamiczne dozowanie (Wariant B + timeout)
5. **Metryki EMA** — obliczanie po każdej dolewce, persystencja w FRAM
6. **Ring buffer TopOffRecord** — zapis, odczyt, obliczanie daily_buckets
7. **Provisioning** — dodanie drugiego kroku z parametrami algorytmu + pre-fill z FRAM
8. **Web API top-off** — `/api/topoff/*` endpointy
9. **Doser core** — portowanie `channel_manager` z `dosing_system-iot` (2 kanały)
10. **Web API doser** — `/api/dose/*` endpointy
11. **GUI** — dashboard: zakładki Top-Off | Historia | Doser A | Doser B

---

## 7. Zależności — bez zmian

```ini
; platformio.ini
lib_deps =
    ESPAsyncWebServer (me-no-dev)
    AsyncTCP (me-no-dev)
    ArduinoJson ^7.4.2
    RTClib ^2.1.1 (Adafruit)
    Adafruit FRAM I2C
```
