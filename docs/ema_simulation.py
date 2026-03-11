#!/usr/bin/env python3
"""
Symulacja algorytmu EMA dla systemu ATO.
Odzwierciedla logikę z water_algorithm.cpp / algorithm_config.h.

Uruchomienie:
    python3 docs/ema_simulation.py
"""

import math

# ---- Parametry (mirror algorithm_config.h defaults) ----
EMA_ALPHA        = 0.20
VOL_YELLOW_PCT   = 30
VOL_RED_PCT      = 60
RATE_YELLOW_PCT  = 40
RATE_RED_PCT     = 80
MIN_BOOTSTRAP    = 5


def clamp_pct(v):
    return max(-127, min(127, round(v)))


def dev_pct(current, ema_val, bootstrap_count):
    if ema_val < 0.001 or bootstrap_count < MIN_BOOTSTRAP:
        return 0
    return clamp_pct((current - ema_val) / ema_val * 100)


def alert_level(dev_vol, dev_rate, bootstrap_count):
    if bootstrap_count < MIN_BOOTSTRAP:
        return 0
    abs_vol  = abs(dev_vol)
    abs_rate = abs(dev_rate)
    lv = 0
    if abs_vol >= VOL_RED_PCT or abs_rate >= RATE_RED_PCT:
        lv = 2
    elif abs_vol >= VOL_YELLOW_PCT or abs_rate >= RATE_YELLOW_PCT:
        lv = 1
    return lv


def run_simulation(events, alpha=EMA_ALPHA, label=""):
    """
    events: lista krotek (volume_ml, interval_s)
    Zwraca listę wyników rekordów.
    """
    print(f"\n{'='*70}")
    print(f"SYMULACJA: {label}  alpha={alpha}")
    print(f"{'='*70}")
    header = f"{'#':>3}  {'vol[ml]':>8}  {'int[h]':>7}  {'rate[ml/h]':>11}  "
    header += f"{'EMA_vol':>8}  {'EMA_rate':>9}  {'dVol%':>6}  {'dRate%':>7}  {'alert':>6}  {'boot':>5}"
    print(header)
    print("-" * 90)

    ema_vol  = 0.0
    ema_int  = 0.0
    ema_rate = 0.0
    bootstrap = 0

    records = []

    for i, (vol, interval_s) in enumerate(events, 1):
        rate = (vol / interval_s * 3600) if interval_s > 0 else 0.0

        if bootstrap == 0:
            ema_vol  = float(vol)
            ema_int  = float(interval_s)
            ema_rate = rate
        else:
            ema_vol  = alpha * vol       + (1 - alpha) * ema_vol
            ema_int  = alpha * interval_s + (1 - alpha) * ema_int
            ema_rate = alpha * rate      + (1 - alpha) * ema_rate

        bootstrap = min(bootstrap + 1, 255)

        dv = dev_pct(vol,  ema_vol,  bootstrap)
        dr = dev_pct(rate, ema_rate, bootstrap)
        al = alert_level(dv, dr, bootstrap)

        ALERT_STR = ["   OK", " ŻÓŁT", "  CZE"]
        alert_s = ALERT_STR[al] if al < 3 else "?????"

        print(f"{i:>3}  {vol:>8}  {interval_s/3600:>7.2f}  {rate:>11.2f}  "
              f"{ema_vol:>8.1f}  {ema_rate:>9.3f}  {dv:>+6}  {dr:>+7}  {alert_s}  {bootstrap:>5}")

        records.append({
            "i": i, "vol": vol, "interval_s": interval_s, "rate": rate,
            "ema_vol": ema_vol, "ema_rate": ema_rate,
            "dev_vol": dv, "dev_rate": dr, "alert": al, "bootstrap": bootstrap
        })

    return records


# ======================================================================
# SCENARIUSZ 1 — Stabilna praca, typowe dolewki co 6h
# ======================================================================
stable = [(120, 6*3600)] * 10

# ======================================================================
# SCENARIUSZ 2 — Normalna praca, potem anomalia (np. dziura w zbiorniku)
# ======================================================================
anomaly = [(120, 6*3600)] * 6 + \
          [(350, 1*3600)] * 4   # bardzo duże, bardzo częste dolewki

# ======================================================================
# SCENARIUSZ 3 — Budowanie EMA od zera z losowym szumem (typowe pierwsze dni)
# ======================================================================
import random
random.seed(42)
buildup = [(round(random.gauss(130, 15)), round(random.gauss(6*3600, 1800))) for _ in range(15)]

# ======================================================================
# SCENARIUSZ 4 — Porównanie alpha (jak szybko EMA "zapomina" stare wartości)
# ======================================================================
alpha_test_events = [(120, 6*3600)] * 5 + [(300, 2*3600)] * 5 + [(120, 6*3600)] * 5


if __name__ == "__main__":
    run_simulation(stable,  label="Stabilna praca (120ml co 6h × 10)")
    run_simulation(anomaly, label="Normalne × 6 + anomalia × 4")
    run_simulation(buildup, label="Budowanie od zera — losowy szum")

    print(f"\n{'='*70}")
    print("PORÓWNANIE ALPHA — reakcja na anomalię (120ml × 5, potem 300ml × 5)")
    print(f"{'='*70}")
    for a in [0.10, 0.20, 0.40]:
        r = run_simulation(alpha_test_events, alpha=a, label=f"alpha={a}")
        # Pokaż kiedy alert się włącza
        first_alert = next((rec for rec in r if rec["alert"] > 0), None)
        if first_alert:
            print(f"  → alpha={a}: pierwszy alert przy dolewce #{first_alert['i']} "
                  f"(dev_vol={first_alert['dev_vol']:+d}%)")
        else:
            print(f"  → alpha={a}: brak alertów")

    print()
    print("Uruchomienie zakończone. Dostosuj scenariusze w ema_simulation.py.")
