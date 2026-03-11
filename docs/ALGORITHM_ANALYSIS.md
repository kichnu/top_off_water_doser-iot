# Analiza algorytmu ATO — wnioski robocze

> Plik aktualizowany na bieżąco podczas pracy. Ostatnia aktualizacja: sesja 2026-03-10

---

## 1. Wartości startowe (domyślne — FRAM pusty)

### PumpSettings (config.h)
| Parametr | Wartość | Uwagi |
|---|---|---|
| `volumePerSecond` | **1.0 ml/s** | Wymaga kalibracji przed uruchomieniem! |
| `manualCycleSeconds` | 60 s | Czas ręcznego cyklu z GUI |
| `calibrationCycleSeconds` | 30 s | Czas cyklu kalibracyjnego |

### TopOffConfig (algorithm_config.h)
| Parametr | Wartość | Znaczenie |
|---|---|---|
| `max_dose_ml` | **300 ml** | Limit bezpieczeństwa jednej dolewki |
| `ema_alpha` | **0.20** | Wygładzanie EMA (nowa wartość waży 20%) |
| `vol_yellow_pct` | 30% | Alert żółty — odchylenie objętości |
| `vol_red_pct` | 60% | Alert czerwony — odchylenie objętości |
| `rate_yellow_pct` | 40% | Alert żółty — odchylenie tempa |
| `rate_red_pct` | 80% | Alert czerwony — odchylenie tempa |
| `history_window_s` | 86400 s | Okno rolling 24h |
| `DEFAULT_MIN_BOOTSTRAP` | **5** | Min. dolewek zanim alerty są aktywne |

### Debouncing
| Parametr | Wartość | Efektywny czas |
|---|---|---|
| `DEBOUNCE_INTERVAL_MS` | 200 ms | — |
| `DEBOUNCE_COUNTER` | 5 | 5 × 200 ms = **1000 ms** |
| `RELEASE_CHECK_INTERVAL_MS` | 500 ms | — |
| `RELEASE_DEBOUNCE_COUNT` | 3 | 3 × 500 ms = **1500 ms** po zwolnieniu |

### Czas pracy pompy (timeout bezpieczeństwa)
```
maxPumpDurationMs = max_dose_ml / volumePerSecond × 1000
= 300 / 1.0 × 1000 = 300 000 ms = 300 s (5 minut)
```
**Po kalibracji np. 3 ml/s:** 300 / 3.0 = 100 s

---

## 2. Mechanizm EMA — jak działa

### Wzór
```
Pierwsze zdarzenie (bootstrap_count == 0):
    EMA = wartość_bezpośrednia

Kolejne:
    EMA_nowa = alpha × wartość + (1 - alpha) × EMA_stara
    EMA_nowa = 0.20 × wartość + 0.80 × EMA_stara
```

### Trzy śledzane wartości
- `ema_volume_ml` — typowa objętość jednej dolewki
- `ema_interval_s` — typowy czas między dolewkami
- `ema_rate_ml_h` — typowe tempo zużycia wody [ml/h]

### Przykład zbieżności (alpha=0.20)
| Dolewka # | Wartość [ml] | EMA [ml] | Odchylenie |
|---|---|---|---|
| 1 | 120 | 120.0 | 0% (bootstrap) |
| 2 | 115 | 119.0 | -3.4% (bootstrap) |
| 3 | 125 | 120.2 | +4.0% (bootstrap) |
| 4 | 118 | 119.8 | -1.5% (bootstrap) |
| 5 | 120 | 119.8 | +0.2% (bootstrap) |
| 6 | **200** | **135.8** | **+47%** ← **ŻÓŁTY ALERT** |
| 7 | **250** | **158.7** | **+57%** ← nadal żółty |

> **Wniosek:** Po 5 dolewkach EMA powoli "przesunie się" w kierunku nowej normy.
> Alert żółty przy +30%, czerwony przy +60% od aktualnego EMA.

### Wzór odchylenia
```
dev_pct = (bieżąca - EMA) / EMA × 100
```
Zakres: int8_t [-127, +127]. Alerty **wyłączone** gdy `bootstrap_count < 5`.

---

## 3. Jak testować

### Opcja A — Python simulation (ZALECANA do logiki EMA)
Plik: `docs/ema_simulation.py` (do stworzenia)

Extrakcja czystej matematyki z C++ do Pythona. Można symulować:
- Budowanie EMA od zera
- Wstrzyknięcie anomalii (duże dawki, skrócone/wydłużone interwały)
- Zbieżność z różnymi alpha (0.10, 0.20, 0.40)
- Kiedy alarm się włącza i wygasa

### Opcja B — Serial monitor na sprzęcie
Algorytm loguje każdy krok via `LOG_INFO`:
```
EMA: vol=119.8 int=3600s rate=0.12 ml/h (bootstrap=5)
METRICS: dev_vol=+47% dev_rate=+20% alert=1 bootstrap=5/5
```
Wystarczy przyłożyć czujnik i obserwować serial @ 115200.

### Opcja C — PlatformIO native unit tests
Wymaga mockowania: `millis()`, `digitalRead()`, `digitalWrite()`,
`getUnixTimestamp()`, FRAM, RTC. Dużo roboty — sensowne dopiero gdy
zbudujemy warstwę HAL/mock.

---

## 3b. Wyniki symulacji (ema_simulation.py — 2026-03-10)

### Stabilna praca
EMA idealnie śledzi wartość stałą — odchylenia 0%, brak alertów. ✓

### Anomalia (scenariusz awarii: 350ml co 1h zamiast 120ml co 6h)
| Dolewka | Objętość | Alert | Wniosek |
|---|---|---|---|
| 6 (1. anomalia) | 350ml | **CZERWONY +111%** | Natychmiastowa detekcja |
| 9 | 350ml | **CZERWONY +51%** | EMA "goni" nową wartość |
| 10 | 350ml | ŻÓŁTY +37% | EMA już blisko anomalii → mniej czuły |

> **Wniosek:** Przy trwałej anomalii EMA adaptuje się po ~8 zdarzeniach i alarm zanika.
> To **poprawne zachowanie** — system "uznaje" nowe tempo za normę. Jeśli to awaria
> (czujnik utknięty LOW), blokuje limit dobowy (do zaimplementowania — Problem 1).

### Porównanie alpha — jak szybko EMA "zapomina"
| alpha | Pierwszy alert | Powrót do OK po anomalii | Charakterystyka |
|---|---|---|---|
| **0.10** | Dolewka #6 | **Nie wraca** (w 15 dolewkach nadal ŻÓŁTY) | Wolno zapomina, konserwatywny |
| **0.20** | Dolewka #6 | Nie wraca w 15 | Balans — **zalecany default** |
| **0.40** | Dolewka #6 | Wraca po ~5 normalnych | Szybko adaptuje, mniej czuły |

> **Wniosek:** Wszystkie alpha wykrywają anomalię przy tej samej dolewce.
> Różnica to zachowanie **po** anomalii. alpha=0.10 długo "pamięta" — generuje
> fałszywe alerty gdy środowisko się zmieni (lato/zima). alpha=0.40 zbyt szybko
> adaptuje się do anomalii tracąc czujność. **Default 0.20 jest właściwy.**

### Budowanie od zera (szum ±15ml, ±30min)
Po 5 dolewkach alerty aktywne, ale naturalne odchylenia (±8%) nie przekraczają progu 30%.
Progi 30%/60% są odpowiednio luźne dla normalnej pracy. ✓

---

## 4. Znalezione problemy / luki

### PROBLEM 1: Brak twardego limitu dobowego (24h)
**Plik:** `water_algorithm.cpp:194`
```cpp
if (rolling24hVolumeMl >= config.max_dose_ml * 10) {
    LOG_WARNING("ALGORITHM: 24h limit check — rolling=%d ml", rolling24hVolumeMl);
}
// Komentarz w kodzie: "Właściwy limit dobowy jako parametr konfiguracyjny — do dodania w v2"
```
**Efekt:** Pompa startuje zawsze — limit dobowy tylko loguje ostrzeżenie, nie blokuje.
**Fix:** Dodać `daily_max_ml` do `TopOffConfig` (np. DEFAULT 2000 ml) i blokować start gdy przekroczony.

### PROBLEM 2: `addManualVolume()` nie zapisuje rekordu do FRAM
**Plik:** `water_algorithm.cpp:586`
```cpp
void WaterAlgorithm::addManualVolume(uint16_t volumeMl) {
    rolling24hVolumeMl += volumeMl;  // tylko cache RAM — nie ma rekordu FRAM!
}
```
**Efekt:** Objętości ręcznych dolewek znikają po restarcie. EMA nie uwzględnia ręcznych dolewek.

### PROBLEM 3: `scanRolling24h()` ładuje 60 rekordów z FRAM przy każdym wywołaniu
**Plik:** `water_algorithm.cpp:424`
Wywoływane w: `onDebounceComplete()`, `finishPumpCycle()`, `initFromFRAM()`
Każdy odczyt = 1200 bajtów przez I2C. Na ESP32-C3 @ I2C 400kHz = ~24ms.
**Nie krytyczne** przy obecnej częstości, ale warto mieć świadomość.

### PROBLEM 4: `flow_rate = 1.0 ml/s` to wartość nieskalibrowana
Domyślna wartość `volumePerSecond = 1.0` daje:
- Timeout bezpieczeństwa = 300s (5 minut!) — zbyt długi dla małej pompy
- Objętość liczona błędnie dopóki nie wykonana kalibracja
**Fix:** Przed pierwszym uruchomieniem wykonać kalibrację z GUI (`MANUAL_EXTENDED` przez 30s, zmierzyć fizycznie objętość).

### PROBLEM 5: `checkPumpAutoEnable()` wywoływane w `update()` — niejasna zależność
Funkcja zdefiniowana w `config.cpp`, wywoływana z algorytmu. Sprawdza 30-minutowy auto-re-enable systemu. Mechanizm działa poprawnie ale jest "ukryty" w algorytmie.

---

## 5. Parametry do ustawienia przed produkcją

Kolejność:
1. **Kalibracja pompy** — uruchom `MANUAL_EXTENDED` przez 30s, zmierz fizycznie ml → oblicz `volumePerSecond`
2. **Ustaw `max_dose_ml`** — np. 150 ml dla akwarium 100l (uzależnione od typowego parowania)
3. **Dodaj `daily_max_ml`** — np. 1500 ml (zabezpieczenie przed awaria czujnika)
4. Pozostałe (progi EMA, alpha) — można zostawić defaulty i korygować po 2-3 tygodniach danych

---

## 6. Rolling 24h — co liczy, czego nie liczy

**Liczy:** wszystkie `TopOffRecord` z FRAM w oknie `now - history_window_s`
**Nie liczy:** ręcznych dolewek (`addManualVolume` — patrz Problem 2)
**Nie liczy:** dolewek sprzed poprzedniego restartu jeśli RTC nie miał czasu UTC
(przy pierwszym uruchomieniu bez NTP timestamp = 0 → rekordy mogą mieć timestamp=0)
