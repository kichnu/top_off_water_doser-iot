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
- `DEBOUNCE_INTERVAL_MS = 200` — okres jednego próbkowania
- `DEBOUNCE_COUNTER = 5` — liczba kolejnych zgodnych odczytów wymagana do zmiany stanu
- Efektywny czas debounce = `200 ms × 5 = 1000 ms`

Parametry są stałymi czasu kompilacji w `algorithm_config.h` — nie konfigurowalne przez użytkownika.

**RELEASE VERIFICATION** — pozostaje, dotyczy jednego pinu (GPIO 3):
- `RELEASE_CHECK_INTERVAL_MS = 500` — co ile sprawdzać podczas pompowania
- `RELEASE_DEBOUNCE_COUNT = 3` — ile kolejnych HIGH wymagane do zatrzymania pompy

**Logika debounce:**
```
Każde DEBOUNCE_INTERVAL_MS:
  odczytaj GPIO 3
  jeśli LOW (woda poniżej poziomu):
    counter++
  else:
    counter = 0  ← reset przy niestabilnym sygnale
  jeśli counter >= DEBOUNCE_COUNTER:
    → onDebounceComplete() → startuj pompę
```

### 2.2 Dynamiczne dozowanie (Wariant B + timeout)

Pompa nie pracuje przez stały czas — pracuje **aż czujnik wróci do HIGH**.

```
FLOW:
  1. Czujnik → stabilnie LOW (po debounce)
  2. Sprawdź rolling 24h — jeśli >= limit → ERROR_DAILY_LIMIT, abort
  3. Uruchom pompę (max czas = max_dose_ml / flow_rate_ml_s)
  4. Czekaj na czujnik HIGH × RELEASE_DEBOUNCE_COUNT  ←── warunek stopu
     LUB upłynął MAX_DOSE_TIME_S                       ←── bezpiecznik
  5. Zatrzymaj pompę
  6. Oblicz faktyczną objętość: volume_ml = duration_s × flow_rate_ml_s
  7. Zapisz TopOffRecord do FRAM
  8. Zaktualizuj EMA i cache rolling 24h
  9. Jeśli timeout → flaga FLAG_TIMEOUT → alarm
```

**Parametry (konfigurowane przez użytkownika, przechowywane w FRAM):**

| Parametr | Rola | Zakres | Domyślna |
|----------|------|--------|---------|
| `max_dose_ml` | Górny limit bezpieczeństwa jednej dolewki; gdy przekroczony → timeout + alarm | 50–2000 ml | 300 ml |
| `flow_rate_ml_s` | Wydajność pompy top-off [ml/s] z kalibracji | kalibracja w GUI | konfigurowalny |

`MAX_DOSE_TIME_S` wyliczany dynamicznie: `max_dose_ml / flow_rate_ml_s` (nie hardkodowany).

### 2.3 Struktura rekordu dolewki — `TopOffRecord`

Rozmiar: **20 bajtów / rekord** (zweryfikowane `static_assert`).

```cpp
struct TopOffRecord {
    uint32_t timestamp;       // Unix UTC z DS3231 RTC              [offset 0]
    uint32_t interval_s;      // Czas od poprzedniej dolewki [s]    [offset 4]
    float    rate_ml_h;       // Tempo zużycia wody [ml/h]          [offset 8]
    uint16_t volume_ml;       // Faktyczna objętość = duration × flow_rate  [offset 12]
    int8_t   dev_volume_pct;  // Odchylenie obj. od EMA [%], -127..+127    [offset 14]
    int8_t   dev_rate_pct;    // Odchylenie rate od EMA [%]                [offset 15]
    uint8_t  alert_level;     // 0 = OK, 1 = ŻÓŁTY, 2 = CZERWONY          [offset 16]
    uint8_t  flags;           // Bitmaska flag                             [offset 17]
    uint8_t  _pad[2];         // Wyrównanie do wielokrotności 4B           [offset 18]
};
// sizeof = 20 — static_assert verified

// flags bitmask:
// bit 0 — FLAG_MANUAL    (dolewka wyzwolona ręcznie z GUI)
// bit 1 — FLAG_TIMEOUT   (pompa zatrzymana przez timeout)
// bit 2 — FLAG_BOOTSTRAP (rekord z okresu budowania historii EMA)
```

### 2.4 Ring buffer — okno czasowe 24–48h

Klasyczny ring buffer z write pointer. Aktywne przy obliczaniu metryk są tylko rekordy
z `timestamp > now - history_window_s`.

| Parametr | Wartość |
|----------|---------|
| `TOPOFF_HISTORY_SIZE` | 60 slotów |
| `FRAM_TOPOFF_RECORD_SIZE` | 20 bajtów |
| Rozmiar w FRAM | 60 × 20 B = **1200 bajtów** |
| `history_window_s` | 86400 (24h) domyślnie; konfigurowalne: 86400 / 129600 / 172800 |

Gdy buffer pełny → nadpisywany najstarszy rekord (write pointer mod 60).

### 2.5 Metryki trendów — EMA + odchylenia

Obliczane **po każdej dolewce**, persystowane w FRAM (blok EMA).

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

**Bootstrap:** Pierwsze `DEFAULT_MIN_BOOTSTRAP = 5` zdarzeń budują EMA bez alertów.
GUI może informować: "Budowanie historii: 3/5". Rekordy z tego okresu mają flagę `FLAG_BOOTSTRAP`.

#### Odchylenia procentowe

```
dev_volume_pct = (volume_ml - ema_volume_ml) / ema_volume_ml × 100
dev_rate_pct   = (rate_ml_h - ema_rate_ml_h) / ema_rate_ml_h × 100
```

Wynik zaciskany do zakresu `int8_t` (-127..+127) przez `clampDevPct()`.

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
Niezależnie: `FLAG_TIMEOUT` zawsze skutkuje poziomem CZERWONY.

#### Blok EMA w FRAM — persystencja przez restart

```cpp
struct EmaBlock {           // sizeof = 16 — static_assert verified
    float   ema_volume_ml;    // 4B
    float   ema_interval_s;   // 4B
    float   ema_rate_ml_h;    // 4B
    uint8_t bootstrap_count;  // 1B — liczba zebranych dolewek (do DEFAULT_MIN_BOOTSTRAP)
    uint8_t _pad[3];          // 3B — wyrównanie
};
```

---

## 3. Konfiguracja parametrów — Provisioning + GUI

### Zasada: defaults w kodzie, nadpisania w FRAM

```
Przy starcie systemu:
  1. Odczytaj TopOffConfig z FRAM
  2. Jeśli is_configured == 0xA5 (TOPOFF_CONFIG_MAGIC) → użyj wartości z FRAM
  3. Jeśli nie → użyj wartości domyślnych z algorithm_config.h
```

Domyślne wartości są zawsze w kodzie (`algorithm_config.h`). FRAM przechowuje tylko
to, co użytkownik świadomie zmienił. Aktualizacja firmware dostarcza nowe wartości
domyślne bez potrzeby migracji FRAM.

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
| `ema_alpha` | FRAM jeśli istnieje, **domyślna z kodu** jeśli nie | |
| Progi alertów | FRAM jeśli istnieje, **domyślne z kodu** jeśli nie | |
| `history_window_s` | FRAM jeśli istnieje, **domyślna z kodu** jeśli nie | |

### Domyślne wartości w kodzie (`algorithm_config.h`)

```cpp
// Debouncing — stałe czasu kompilacji (nie w FRAM)
#define DEBOUNCE_INTERVAL_MS     200   // ms między próbkowaniami
#define DEBOUNCE_COUNTER           5   // kolejnych LOW wymaganych (= 1000 ms efektywnie)
#define RELEASE_CHECK_INTERVAL_MS  500
#define RELEASE_DEBOUNCE_COUNT       3

// Domyślne wartości konfiguracji (mogą być nadpisane przez FRAM)
#define DEFAULT_MAX_DOSE_ML         300     // limit bezpieczeństwa jednej dolewki [ml]
#define DEFAULT_EMA_ALPHA          0.20f    // zakres: 0.10–0.40
#define DEFAULT_VOL_YELLOW_PCT       30     // [%]
#define DEFAULT_VOL_RED_PCT          60     // [%]
#define DEFAULT_RATE_YELLOW_PCT      40     // [%]
#define DEFAULT_RATE_RED_PCT         80     // [%]
#define DEFAULT_HISTORY_WINDOW_S  86400     // [s] = 24h
#define DEFAULT_MIN_BOOTSTRAP         5     // min dolewek przed aktywacją alertów
```

### Dwupoziomowy dostęp do parametrów

| Parametr | Provisioning | GUI (dashboard) |
|----------|:---:|:---:|
| WiFi SSID | ✓ | — |
| WiFi hasło | ✓ | — |
| Hasło admina | ✓ | — |
| Nazwa urządzenia | ✓ | — |
| `max_dose_ml` | ✓ | ✓ |
| `ema_alpha` | ✓ | ✓ |
| `vol_yellow_pct` / `vol_red_pct` | ✓ | ✓ |
| `rate_yellow_pct` / `rate_red_pct` | ✓ | ✓ |
| `history_window_s` | ✓ | ✓ |
| Kalibracja pompy top-off (`flow_rate_ml_s`) | — | ✓ |
| Konfiguracja kanałów dosera | — | ✓ |

### API endpoints — aktualne (zaimplementowane)

```
GET  /api/status              → stan systemu (sensor, pompa, RTC, WiFi, EMA state)
GET  /api/cycle-history       → ring buffer TopOffRecord (60 rekordów, newest-first)
GET  /api/daily-volume        → rolling 24h stats (rolling_24h_ml, history_window_s, max_dose_ml)
GET  /api/fill-water-max      → pobierz max_dose_ml
POST /api/fill-water-max      → ustaw max_dose_ml (50–2000 ml)
GET  /api/get-statistics      → statystyki błędów z FRAM (gap1, gap2, water fails)
POST /api/reset-statistics    → zeruj statystyki błędów
POST /api/pump/normal         → ręczna dolewka
POST /api/pump/stop           → zatrzymaj pompę
GET  /api/pump-settings       → konfiguracja pompy (volume_per_second, cycle times)
POST /api/pump-settings       → zapisz konfigurację pompy
```

### API endpoints — planowane (TODO)

```
GET  /api/topoff/settings     → pełna TopOffConfig (alpha, progi, history_window_s)
POST /api/topoff/settings     → zapisz TopOffConfig do FRAM
GET  /api/topoff/status       → bieżący stan: EMA values, bootstrap_count, alert_level
POST /api/topoff/calibrate    → start/stop kalibracji pompy (flow_rate_ml_s)
GET  /api/dose/status         → stan kanałów dozownika
POST /api/dose/manual         → ręczne wyzwolenie kanału
GET  /api/dose/settings       → konfiguracja kanałów
POST /api/dose/settings       → zapis konfiguracji
```

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

---

## 5. FRAM — layout pamięci (rzeczywiste adresy)

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
0x0518 – 0x0519   daily_volume_ml — legacy (2 B)
0x051A – 0x051D   last_reset_utc_day — legacy (4 B)
0x051E – 0x051F   daily checksum — legacy (2 B)
0x0520 – 0x0529   avail_vol_max + current — legacy, unused (10 B)
0x052A – 0x052D   fill_water_max — legacy, unused (4 B)
0x052E – 0x052F   fill_max checksum — legacy (2 B)
0x0530 – 0x0531   cycle_count — legacy (2 B)
0x0532 – 0x0533   cycle_index — legacy (2 B)

0x0560 – 0x0573   TopOffConfig struct (20 B)  ← sizeof = 20, static_assert verified
0x0574 – 0x0575   TopOffConfig checksum (2 B)

0x0580 – 0x058F   EmaBlock struct (16 B)      ← sizeof = 16, static_assert verified
0x0590 – 0x0591   EmaBlock checksum (2 B)

0x0600 – 0x0601   TopOff record count (2 B)
0x0602 – 0x0603   TopOff write pointer (2 B)
0x0610 – 0x0AC0   TopOff ring buffer: 60 × 20 B = 1200 B

0x1000 – ...      [RESERVED] Doser: konfiguracja kanałów CH1 + CH2
```

> Pełna lista stałych → `src/hardware/fram_controller.h`

---

## 6. Kolejność implementacji

### ✅ Zrealizowane

1. **`hardware_pins.h`** — jeden `WATER_SENSOR_PIN` (GPIO 3), dodane `DOSE_RELAY_PIN_CH1/CH2`, przemianowane piny I2C
2. **`algorithm_config.h`** — nowe parametry debounce, nowe struktury TopOffRecord/EmaBlock/TopOffConfig, stany, kody błędów
3. **`water_sensors.h/cpp`** — uproszczone do jednego czujnika z prostym debouncingiem, callbacki do algorytmu
4. **`water_algorithm.h/cpp`** — nowy algorytm: maszyna stanów (IDLE→DEBOUNCING→PUMPING→LOGGING), dynamiczne dozowanie, EMA, rolling 24h, persist do FRAM
5. **`fram_controller.h/cpp`** — nowe sekcje FRAM: TopOffConfig, EmaBlock, ring buffer TopOffRecord z write pointer
6. **`web_handlers.cpp`** — zaktualizowane handlery do nowego API (TopOffRecord history, rolling 24h, direct FRAM dla statystyk)
7. **`rtc_controller.cpp`** — poprawka nazw pinów I2C
8. **`main.cpp`** — dopasowanie do nowego API

### ⏳ Do zrealizowania

9. **Provisioning** — dodanie drugiego kroku z parametrami algorytmu + pre-fill z FRAM
10. **Web API `/api/topoff/settings`** — endpoint GET/POST dla pełnej TopOffConfig
11. **Doser core** — portowanie `channel_manager` z `dosing_system-iot` (2 kanały)
12. **Web API `/api/dose/*`** — endpointy dozownika
13. **GUI** — dashboard: zakładki Top-Off | Historia | Doser A | Doser B; Chart.js dla EMA

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











  Algorytm harmonogramu (RTC)

  Inicjalizacja:
    ledc_setup(freq, resolution)  ← częstotliwość = docelowe RPM, raz na starcie
    ledc_write(duty=0)            ← silnik zatrzymany
    EN = HIGH                     ← TMC2209 w trybie sleep

  Harmonogram (callback co np. 1h lub na żądanie):
    ┌─ oblicz ml_do_dozowania (na podstawie czujnika/harmonogramu)
    ├─ przelicz czas_pracy = ml / przepływ_ml_per_s
    ├─ EN = LOW              ← wybudź sterownik
    ├─ delay 1ms             ← czas na inicjalizację TMC2209
    ├─ ledc_write(duty=127)  ← START: LEDC generuje STEP automatycznie
    ├─ odpal timer (one-shot, czas_pracy sekund)
    └─ wróć do loop() [loop nic nie robi dla silnika!]

  Callback timera (po czas_pracy):
    ├─ ledc_write(duty=0)    ← STOP: brak pulsów
    ├─ delay 1ms
    ├─ EN = HIGH             ← TMC2209 sleep
    └─ zapisz log (RTC timestamp, ml, status)

  Pętla główna nie zawiera żadnego kodu silnika — tylko esp_timer (one-shot) i LEDC.

  ---
  Dlaczego nie millis()/micros() + loop()

  ┌──────────────────┬────────┬────────────────┬──────────────────────────┐
  │      Metoda      │ Jitter │ Obciążenie CPU │      Problem z WiFi      │
  ├──────────────────┼────────┼────────────────┼──────────────────────────┤
  │ micros() w loop  │ duży   │ stały polling  │ kroки tracone podczas TX │
  ├──────────────────┼────────┼────────────────┼──────────────────────────┤
  │ Timer przerwanie │ mały   │ minimalne ISR  │ OK                       │
  ├──────────────────┼────────┼────────────────┼──────────────────────────┤
  │ LEDC (hardware)  │ zerowy │ zero           │ odporny                  │
  └──────────────────┴────────┴────────────────┴──────────────────────────┘