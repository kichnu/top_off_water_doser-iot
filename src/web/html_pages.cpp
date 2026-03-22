

#include "html_pages.h"

const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Top Off Water - Login</title>
    <style>
        :root {
            --bg-primary: #0a0f1a;
            --bg-card: #111827;
            --bg-input: #1e293b;
            --border: #2d3a4f;
            --text-primary: #f1f5f9;
            --text-secondary: #94a3b8;
            --text-muted: #64748b;
            --accent-blue: #38bdf8;
            --accent-cyan: #22d3d5;
            --accent-green: #22c55e;
            --accent-red: #ef4444;
            --radius: 12px;
            --radius-sm: 8px;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
        }

        body::before {
            content: '';
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: 
                radial-gradient(ellipse at 20% 0%, rgba(56, 189, 248, 0.08) 0%, transparent 50%),
                radial-gradient(ellipse at 80% 100%, rgba(34, 211, 213, 0.06) 0%, transparent 50%);
            pointer-events: none;
            z-index: 0;
        }

        .login-box {
            background: var(--bg-card);
            border: 1px solid var(--border);
            padding: 40px;
            border-radius: var(--radius);
            box-shadow: 0 4px 24px rgba(0,0,0,0.4);
            width: 100%;
            max-width: 400px;
            margin: 20px;
            position: relative;
            z-index: 1;
        }

        .logo {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 12px;
            margin-bottom: 30px;
        }

        .logo-icon {
            width: 40px;
            height: 40px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            border-radius: var(--radius-sm);
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .logo-icon svg {
            width: 24px;
            height: 24px;
            fill: var(--bg-primary);
        }

        h1 {
            font-size: 1.25rem;
            font-weight: 700;
        }

        h1 span {
            color: var(--text-muted);
            font-weight: 500;
        }

        .form-group {
            margin-bottom: 20px;
        }

        label {
            display: block;
            margin-bottom: 8px;
            font-weight: 500;
            color: var(--text-secondary);
            font-size: 0.875rem;
        }

        input[type="password"] {
            width: 100%;
            padding: 12px 16px;
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            font-size: 1rem;
            color: var(--text-primary);
        }

        input[type="password"]:focus {
            outline: none;
            border-color: var(--accent-blue);
            box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.2);
        }

        .login-btn {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            color: var(--bg-primary);
            border: none;
            border-radius: var(--radius-sm);
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }

        .login-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(56, 189, 248, 0.3);
        }

        .alert {
            padding: 12px 16px;
            margin: 15px 0;
            border-radius: var(--radius-sm);
            display: none;
            font-size: 0.875rem;
        }

        .alert.error {
            background: rgba(239, 68, 68, 0.15);
            border: 1px solid rgba(239, 68, 68, 0.3);
            color: var(--accent-red);
        }

        .info {
            margin-top: 24px;
            padding: 16px;
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            font-size: 0.75rem;
            color: var(--text-muted);
        }

        .info strong {
            color: var(--text-secondary);
        }
    </style>
</head>
<body>
    <div class="login-box">
        <div class="logo">
            <div class="logo-icon">
                <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
            </div>
            <h1>Top Off Water <span>– System</span></h1>
        </div>
        <form id="loginForm">
            <div class="form-group">
                <label for="password">Administrator Password</label>
                <input type="password" id="password" name="password" required />
            </div>
            <button type="submit" class="login-btn">Login</button>
        </form>
        <div id="error" class="alert error"></div>
        <div class="info">
            <strong>Security Features:</strong><br />
            • Session-based authentication<br />
            • Rate limiting & IP filtering<br />
            • VPS event logging
        </div>
    </div>
    <script>
        document.getElementById("loginForm").addEventListener("submit", function(e) {
            e.preventDefault();
            const password = document.getElementById("password").value;
            const errorDiv = document.getElementById("error");

            fetch("api/login", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "password=" + encodeURIComponent(password),
            })
            .then((response) => response.json())
            .then((data) => {
                if (data.success) {
                    window.location.href = "./";
                } else {
                    errorDiv.textContent = data.error || "Login failed";
                    errorDiv.style.display = "block";
                }
            })
            .catch((error) => {
                errorDiv.textContent = "Connection error - Check if device is running";
                errorDiv.style.display = "block";
            });
        });
    </script>
</body>
</html>
)rawliteral";

const char* DASHBOARD_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Top Off Water</title>
    <style>
        :root {
            --bg-primary: #0a0f1a;
            --bg-card: #111827;
            --bg-card-hover: #1a2332;
            --bg-input: #1e293b;
            --border: #2d3a4f;
            --text-primary: #f1f5f9;
            --text-secondary: #94a3b8;
            --text-muted: #64748b;
            --accent-blue: #38bdf8;
            --accent-cyan: #22d3d5;
            --accent-green: #22c55e;
            --accent-red: #ef4444;
            --accent-orange: #f97316;
            --accent-yellow: #eab308;
            --shadow: 0 4px 24px rgba(0,0,0,0.4);
            --radius: 12px;
            --radius-sm: 8px;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            line-height: 1.5;
        }

        body::before {
            content: '';
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: 
                radial-gradient(ellipse at 20% 0%, rgba(56, 189, 248, 0.08) 0%, transparent 50%),
                radial-gradient(ellipse at 80% 100%, rgba(34, 211, 213, 0.06) 0%, transparent 50%);
            pointer-events: none;
            z-index: 0;
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 16px;
            position: relative;
            z-index: 1;
        }

        /* Header */
        header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 12px 0 24px;
        }

        .logo {
            display: flex;
            align-items: center;
            gap: 12px;
        }

        .logo-icon {
            width: 40px;
            height: 40px;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-blue));
            border-radius: var(--radius-sm);
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .logo-icon svg {
            width: 24px;
            height: 24px;
            fill: var(--bg-primary);
        }

        h1 {
            font-size: 1.25rem;
            font-weight: 700;
            letter-spacing: -0.02em;
        }

        h1 span {
            color: var(--text-muted);
            font-weight: 500;
        }
        

        .btn-back {
            background: var(--bg-input);
            border: 1px solid var(--border);
            color: var(--text-secondary);
            padding: 8px 16px;
            border-radius: var(--radius-sm);
            font-size: 0.875rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s;
        }

        .btn-back:hover {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        /* Cards */
        .card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            padding: 20px;
            margin-bottom: 16px;
            box-shadow: var(--shadow);
        }

        .card-header {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 16px;
            padding-bottom: 12px;
            border-bottom: 1px solid var(--border);
        }

        .card-header h2 {
            font-size: 0.9rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--text-secondary);
        }

        .stat-column h3{
            font-size: 0.9rem;
            font-weight: 600;
            text-align: center;
            letter-spacing: 0.05em;
            color: var(--text-secondary);
        }

        .card-header-icon {
            width: 28px;
            height: 28px;
            border-radius: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .card-header-icon svg {
            width: 16px;
            height: 16px;
        }

        /* ===== FIRST CARD: System Status ===== */
        .status-main {
            display: flex; align-items: center; justify-content: space-between;
            background: var(--bg-input); border: 1px solid var(--border);
            border-radius: var(--radius-sm); padding: 12px 16px;
            gap: 12px; position: relative;
        }
        .status-main.status-ok {
            border-color: rgba(34,197,94,0.35); background: rgba(34,197,94,0.05);
        }
        .status-main.status-ok::after {
            content: ''; position: absolute; top: 8px; right: 8px;
            width: 6px; height: 6px; background: var(--accent-green);
            border-radius: 50%; animation: pulse 2s infinite;
        }
        .status-main.status-error {
            border-color: rgba(239,68,68,0.4); background: rgba(239,68,68,0.06);
        }
        .status-main.status-warn {
            border-color: rgba(234,179,8,0.4); background: rgba(234,179,8,0.06);
        }
        .status-main.status-disabled {
            border-color: rgba(148,163,184,0.35); background: rgba(148,163,184,0.05);
        }
        .status-main-body { flex: 1; min-width: 0; }
        .status-main-desc { font-size: 0.9rem; font-weight: 600; color: var(--text-primary); }
        .status-main-sub {
            display: flex; flex-wrap: wrap; gap: 8px;
            margin-top: 5px; font-size: 0.72rem;
        }
        .sub-on     { color: var(--accent-green); }
        .sub-off    { color: var(--text-muted); }
        .sub-warn   { color: var(--accent-yellow); }
        .sub-danger { color: var(--accent-red); }
        .sub-sep    { color: var(--border); }
        .status-main-wifi {
            display: flex; flex-direction: column; align-items: center;
            gap: 2px; flex-shrink: 0; padding-right: 8px;
        }
        .wifi-label { font-size: 0.6rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; }
        .wifi-dot   { font-size: 1.1rem; line-height: 1; }
        .status-main-wifi.wifi-on  .wifi-dot { color: var(--accent-green); }
        .status-main-wifi.wifi-off .wifi-dot { color: var(--text-muted); }
        .status-info {
            display: flex; align-items: center; flex-wrap: wrap; gap: 6px;
            background: var(--bg-input); border: 1px solid var(--border);
            border-radius: var(--radius-sm); padding: 8px 16px;
            font-size: 0.75rem; color: var(--text-secondary);
            font-family: 'Courier New', monospace; margin-top: 10px;
        }
        .status-info .si-sep { color: var(--text-muted); font-family: sans-serif; }
        .status-info.rtc-warn { border-color: rgba(251,191,36,0.35); }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }


        /* ===== SECOND CARD: Pump Control ===== */
        .pump-controls {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 12px;
        }
        @media (max-width: 480px) {
            .pump-controls { grid-template-columns: repeat(2, 1fr); }
        }
        .btn-kalk-off {
            background: rgba(234,179,8,0.08); border: 1px solid rgba(234,179,8,0.3);
            color: var(--accent-yellow);
        }
        .btn-kalk-off:hover:not(:disabled) { background: rgba(234,179,8,0.18); }
        .btn-kalk-on {
            background: rgba(34,197,94,0.15); border: 1px solid rgba(34,197,94,0.3);
            color: var(--accent-green);
        }
        .btn-kalk-on:hover:not(:disabled) { background: rgba(34,197,94,0.25); }

        .btn {
            display: flex;
            align-items: center;
            justify-content: center;
            height: 48px;
            gap: 8px;
            padding: 14px 20px;
            border-radius: var(--radius-sm);
            font-family: inherit;
            font-size: 0.9rem;
            font-weight: 600;
            border: none;
            cursor: pointer;
            transition: all 0.2s;
        }

        .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
            transform: none !important;
        }

        .btn svg {
            width: 18px;
            height: 18px;
        }

        .btn-primary {
            background: rgba(34, 197, 94, 0.15);
            border: 1px solid rgba(34, 197, 94, 0.3);
            color: var(--accent-green);
        }

        .btn-primary:hover:not(:disabled) {
            transform: translateY(-2px);
            box-shadow: 0 2px 8px rgba(34, 197, 94, 0.3);
        }

        /* Stan OFF dla przycisków bistabilnych */
        .btn-off {
            background: var(--bg-input);
            border: 1px solid rgba(34, 197, 94, 0.3);
            color: var(--text-secondary);
        }

        .btn-off:hover:not(:disabled) {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        .btn-secondary {
            background: var(--bg-input);
            border: 1px solid var(--border);
            color: var(--text-secondary);
        }

        .btn-secondary:hover:not(:disabled) {
            background: var(--bg-card-hover);
            color: var(--text-primary);
        }

        .btn-small {
            padding: 10px 16px;
            font-size: 0.8rem;
            margin-top: 10px;
            /* margin-right: 4px; */
        }

        .btn-small:nth-of-type(2){
            margin-left: 10px;
        }

        /* ===== THIRD CARD: Statistics ===== */
        .stats-columns {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 12px;
            align-items: start;
        }

        /* Single Dose: pierwsza kolumna na desktopie (ostatnia w HTML → order:-1) */
        .stat-col-dose {
            order: -1;
        }

        @media (max-width: 600px) {
            .stats-columns {
                grid-template-columns: 1fr;
            }
            /* Na mobile Single Dose wraca na koniec */
            .stat-col-dose {
                order: 3;
            }
        }

        .stat-column {
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 16px;
            display: flex;
            min-height: 245px;
            flex-direction: column;
            gap: 12px;
        }

        .stat-column .btn {
            width: 100%;
        }

        .stat-content {
            display: flex;
            flex-direction: column;
            gap: 6px;
        }

        .stat-errors {
            display: flex;
            flex-direction: row;
            margin: 3px;
        }

         .stat-available{
            display: flex;
            flex-direction: row;
            margin: 3px;
        }

        .stat-daily{
            display: flex;
            flex-direction: row;
            margin: 3px;
        }
        
        .stat-line {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 0.8rem;
        }

        .stat-line .stat-label {
            color: var(--text-muted);
        }

        .stat-line .stat-value {
            font-family: 'Courier New', monospace;
            font-weight: 600;
            color: var(--accent-green);
        }

        .stat-line .stat-value.neutral {
            color: var(--text-primary);
        }

        /* Volume indicator */
        .volume-indicator {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .volume-bar {
            height: 8px;
            background: var(--bg-primary);
            border-radius: 4px;
            overflow: hidden;
        }

        .volume-bar-fill {
            height: 100%;
            width: 0%;
            background: linear-gradient(90deg, var(--accent-cyan), var(--accent-blue));
            border-radius: 4px;
            transition: width 0.3s;
        }

        .volume-text {
            font-family: 'Courier New', monospace;
            font-size: 0.85rem;
            color: var(--text-primary);
            text-align: center;
        }

        /* ===== FOURTH CARD: Pump Settings ===== */
        .settings-row {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 16px;
            align-items: start;
        }

        @media (max-width: 600px) {
            .settings-row {
                grid-template-columns: 1fr;
            }
        }

        .setting-item {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .setting-item.input-group {
            background: var(--bg-input);
            border-radius: var(--radius-sm);
            padding: 12px;
        }

        .setting-item label {
            font-size: 0.75rem;
            font-weight: 500;
            text-transform: uppercase;
            text-align: center;
            letter-spacing: 0.05em;
            color: var(--text-muted);
        }

        input[type="text"],
        input[type="number"] {
            background: var(--bg-primary);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            padding: 10px 14px;
            font-family: 'Courier New', monospace;
            font-size: 1rem;
            color: var(--text-primary);
            width: 100%;
            text-align: center;
        }

        input:focus {
            outline: none;
            border-color: var(--accent-blue);
            box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.2);
        }

        .current-value {
            font-size: 0.75rem;
            color: var(--text-muted);
            text-align: center;
            margin-top: 4px;
        }

        /* Footer */
        .footer-info {
            text-align: center;
            padding: 16px;
            color: var(--text-muted);
            font-size: 0.75rem;
        }

        /* Cycle History Charts */
        .chart-wrap { margin-top: 12px; }
        .chart-sublabel {
            font-size: 0.7rem;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 0.05em;
            margin: 10px 0 4px;
        }
        .chart-sublabel:first-child { margin-top: 0; }
        .chart-rate-scroll {
            overflow-x: hidden;
            width: 100%;
            border-radius: 6px;
        }
        #chartRate {
            width: 100%;
            height: 190px;
            display: block;
            background: #1e293b;
            /* width overridden by JS after data load */
        }
        .chart-scroll-btns {
            display: flex;
            justify-content: center;
            gap: 16px;
            margin-top: 8px;
        }
        .chart-scroll-btn {
            background: rgba(148,163,184,0.08);
            border: 1px solid rgba(148,163,184,0.18);
            color: #94a3b8;
            border-radius: 10px;
            padding: 10px 0;
            font-size: 1.1rem;
            width: 80px;
            min-height: 44px;
            cursor: pointer;
            touch-action: manipulation;
            -webkit-tap-highlight-color: transparent;
        }
        .chart-scroll-btn:active { background: rgba(148,163,184,0.18); }
        .chart-legend {
            display: flex;
            flex-wrap: wrap;
            gap: 12px;
            margin-top: 10px;
            font-size: 0.72rem;
            color: var(--text-muted);
        }

        /* Kalkwasser schedule tiles */
        .kalk-schedule-wrap {
            margin-bottom: 20px; 
        }
        .kalk-section {
            background: var(--bg-input);
            border: 1px solid var(--border);
            border-radius: var(--radius-sm);
            padding: 12px;
            margin-top: 10px;
        }
        .kalk-section-label {
            font-size: 0.7rem; font-weight: 600; color: var(--text-primary);
            text-transform: uppercase; letter-spacing: 0.08em; margin-bottom: 8px;
        }
        .kalk-mix-grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 6px;
        }
        .kalk-dose-grid {
            display: grid;
            grid-template-columns: repeat(8, 1fr);
            gap: 6px;
        }
        @media (max-width: 640px) {
            .kalk-dose-grid { grid-template-columns: repeat(4, 1fr); }
        }
        .kalk-tile {
            background: var(--bg-primary); border: 1px solid var(--border);
            border-radius: 6px; padding: 8px 4px; font-size: 0.78rem;
            font-family: 'Courier New', monospace; font-weight: 600;
            color: var(--text-secondary); cursor: default;
            text-align: center; transition: background 0.2s, color 0.2s;
        }
        .kalk-tile.kalk-ev-done    { background: rgba(74,222,128,0.1); border-color: rgba(74,222,128,0.3); color: #4ade80; }
        .kalk-tile.kalk-ev-active  { background: rgba(251,191,36,0.15); border-color: rgba(251,191,36,0.45); color: #fbbf24; font-weight: 700; }
        .kalk-tile.kalk-ev-pending { color: var(--text-secondary); }
        .kalk-tile.kalk-ev-missed  { color: rgba(248,113,113,0.45); border-color: rgba(248,113,113,0.15); }
        .kalk-alarm-banner {
            background: rgba(239,68,68,0.12); border: 1px solid rgba(239,68,68,0.3);
            color: #f87171; border-radius: 6px; padding: 8px 12px;
            font-size: 0.8rem; font-weight: 600; margin-top: 10px; display: none;
        }

        .card-subheader {
            font-size: 0.9rem; font-weight: 600; color: var(--text-primary);
            padding: 12px 0 4px; border-top: 1px solid var(--border); margin-top: 12px;
        }

    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <header>
            <div class="logo">
                <div class="logo-icon">
                    <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>
                </div>
                <h1>ATO & Kalkwasser Dosing</h1>
            </div>
            <button class="btn-back" onclick="logout()">Back</button>
        </header>

        <!-- FIRST CARD: System Status -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(56, 189, 248, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-blue);" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/></svg>
                </div>
                <h2>System Status</h2>
            </div>

            <div class="status-main status-ok" id="statusMain">
                <div class="status-main-body">
                    <div class="status-main-desc" id="processDescription">IDLE - Waiting for sensors</div>
                    <div class="status-main-sub">
                        <span id="sensor1Badge" class="sub-off">Sensors: OFF</span>
                        <span class="sub-sep">•</span>
                        <span id="pumpBadge" class="sub-off">Pump: IDLE</span>
                        <span class="sub-sep" id="reservoirSep" style="display:none">•</span>
                        <span id="reservoirAlarmBadge" style="display:none"></span>
                    </div>
                </div>
                <div class="status-main-wifi wifi-off" id="wifiItem">
                    <span class="wifi-label">WiFi</span>
                    <span class="wifi-dot" id="wifiStatus">●</span>
                </div>
            </div>
            <div class="status-info" id="rtcItem">
                <span id="rtcTime">—</span><span id="rtcHint"></span>
                <span class="si-sep">•</span>
                <span id="freeHeap">—</span>
                <span class="si-sep">•</span>
                <span id="uptime">—</span>
            </div>
            <!-- Kalkwasser schedule (tile grid) -->
            <div class="kalk-schedule-wrap" id="kalkScheduleWrap">
                <div class="kalk-alarm-banner" id="kalkAlarmBanner">
                    &#9888; No top-off events in last 24h — kalkwasser dose may be too high!
                </div>
                <div class="kalk-section">
                    <div class="kalk-section-label">Kalkwasser Mixing schedule</div>
                    <div class="kalk-mix-grid">
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix0">00:15</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix1">06:15</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix2">12:15</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkMix3">18:15</div>
                    </div>
                </div>
                <div class="kalk-section">
                    <div class="kalk-section-label">Kalkwasser Dosing schedule</div>
                    <div class="kalk-dose-grid">
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose0">02:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose1">03:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose2">04:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose3">05:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose4">08:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose5">09:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose6">10:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose7">11:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose8">14:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose9">15:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose10">16:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose11">17:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose12">20:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose13">21:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose14">22:00</div>
                        <div class="kalk-tile kalk-ev-pending" id="kalkDose15">23:00</div>
                    </div>
                </div>
            </div>
            </div>

        <!-- SECOND CARD: Pump Control -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(34, 197, 94, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-green);" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 14.5v-9l6 4.5-6 4.5z"/></svg>
                </div>
                <h2>System Control</h2>
            </div>

            <div class="pump-controls">
                <button id="systemToggleBtn" class="btn btn-primary" onclick="toggleSystem()">System On</button>
                <button id="kalkwasserBtn" class="btn btn-kalk-off" onclick="toggleKalkwasser()">Kalkwasser OFF</button>
                <button id="manualPumpBtn" class="btn btn-off">Manual Pump OFF</button>
                <button id="mixingPumpBtn" class="btn btn-off" onclick="toggleMixingPump()">Mixing Pump OFF</button>
                <button id="peristalticPumpBtn" class="btn btn-off" onclick="togglePeristalticPump()">Peristaltic OFF</button>
                <button id="systemResetBtn" class="btn btn-secondary" onclick="systemReset()">System Reset</button>
            </div>
        </div>

        <!-- THIRD CARD: Statistics -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(234, 179, 8, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-yellow);" viewBox="0 0 24 24"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zM9 17H7v-7h2v7zm4 0h-2V7h2v10zm4 0h-2v-4h2v4z"/></svg>
                </div>
                <h2>System Setting</h2>
            </div>

 

            <div class="stats-columns">

                <!-- Column 1: Daily Refill Limit -->
                <div class="stat-column">
                    <h3>Daily Refill Limit</h3>
                    <div class="stat-content">
                        <div class="volume-indicator">
                            <div class="volume-bar">
                                <div class="volume-bar-fill" id="volumeBarFill"></div>
                            </div>
                            <div class="volume-text" id="volumeText">0 ml / 2000 ml</div>
                        </div>
                    </div>
                    <div class="input-group" style="margin-top: 8px;">
                        <input type="number" id="dailyLimitInput" min="100" max="10000" step="100" placeholder="ml">
                    </div>
                    <div class="stat-daily">
                        <button class="btn btn-secondary btn-small" onclick="setDailyLimit()">Set</button>
                    </div>
                </div>

                <!-- Column 2: Kalkwasser Dosing Settings -->
                <div class="stat-column">
                    <h3>Kalkwasser Dosing</h3>
                    <div class="stat-content">
                        <div class="volume-indicator">
                            <div class="volume-bar">
                                <div class="volume-bar-fill" id="kalkDoseBarFill"></div>
                            </div>
                            <div class="volume-text" id="kalkDoseBarText">— ml / day</div>
                        </div>
                    </div>
                    <div class="input-group" style="margin-top: 8px;">
                        <input type="number" id="kalkDailyDose" min="1" max="500" step="1" placeholder="ml/day">
                    </div>
                    <div class="stat-daily">
                        <button class="btn btn-secondary btn-small" onclick="saveKalkConfig()">Save</button>
                    </div>
                </div>

                <!-- Column 3: Single Dose — ostatnia w HTML, pierwsza na desktopie (order:-1) -->
                <div class="stat-column stat-col-dose">
                    <h3>Single Dose</h3>
                    <div class="stat-content">
                        <div class="volume-indicator">
                            <div class="volume-bar">
                                <div class="volume-bar-fill" id="doseBarFill"></div>
                            </div>
                            <div class="volume-text" id="doseText">— ml</div>
                        </div>
                    </div>
                    <div class="input-group" style="margin-top: 8px;">
                        <input type="number" id="doseInput" min="50" max="2000" step="10" placeholder="ml">
                    </div>
                    <div class="stat-daily">
                        <button class="btn btn-secondary btn-small" onclick="setDose()">Set</button>
                    </div>
                </div>
            </div>

            
            
        </div>

        <!-- CYCLE HISTORY CARD -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(34, 211, 213, 0.15);">
                    <svg fill="currentColor" style="color: #22d3d5;" viewBox="0 0 24 24"><path d="M13 3c-4.97 0-9 4.03-9 9H1l3.89 3.89.07.14L9 12H6c0-3.87 3.13-7 7-7s7 3.13 7 7-3.13 7-7 7c-1.93 0-3.68-.79-4.94-2.06l-1.42 1.42C8.27 19.99 10.51 21 13 21c4.97 0 9-4.03 9-9s-4.03-9-9-9zm-1 5v5l4.28 2.54.72-1.21-3.5-2.08V8H12z"/></svg>
                </div>
                <h2>Cycle History</h2>
            </div>
            <div style="display:flex;gap:8px;margin-bottom:8px;">
                <button class="btn btn-secondary" onclick="loadCycleHistory()" id="loadCyclesBtn" style="flex:1;">Load History</button>
                <button class="btn btn-secondary" onclick="deleteCycleHistory()" id="deleteCyclesBtn" style="flex:1;color:#f87171;border-color:rgba(248,113,113,0.3);">Delete Data</button>
            </div>
            <div class="chart-wrap">
                <div class="chart-sublabel">Evaporation rate (ml/h) per cycle</div>
                <div id="chartRateScroll" class="chart-rate-scroll">
                    <canvas id="chartRate"></canvas>
                </div>
                <div class="chart-scroll-btns">
                    <button class="chart-scroll-btn" onclick="scrollRateChart(-1)">&#9664;</button>
                    <button class="chart-scroll-btn" onclick="scrollRateChart(1)">&#9654;</button>
                </div>
                <div class="chart-legend">
                    <span><span style="background:#4ade8022;border:1px solid #4ade8055;display:inline-block;width:12px;height:12px;border-radius:2px;vertical-align:middle;"></span> Normal</span>
                    <span><span style="background:#fbbf2422;border:1px solid #fbbf2455;display:inline-block;width:12px;height:12px;border-radius:2px;vertical-align:middle;"></span> Warning</span>
                    <span><span style="background:#f8717122;border:1px solid #f8717155;display:inline-block;width:12px;height:12px;border-radius:2px;vertical-align:middle;"></span> Alarm</span>
                    <span><span style="color:#94a3b8;font-weight:bold;">&#8212;</span> EMA</span>
                </div>
            </div>
        </div>

        <!-- FOURTH CARD: Pump Setting -->
        <div class="card">
            <div class="card-header">
                <div class="card-header-icon" style="background: rgba(249, 115, 22, 0.15);">
                    <svg fill="currentColor" style="color: var(--accent-orange);" viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.31.06-.63.06-.94 0-.31-.02-.63-.06-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.04.31-.06.63-.06.94s.02.63.06.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
                </div>
                <h2>Pump Calibration</h2>
            </div>

            <div class="card-subheader">ATO Pump Calibration</div>
            <div class="settings-row">
                <div class="setting-item">
                    <button id="extendedBtn" class="btn btn-secondary" onclick="triggerExtendedPump()">Start Calibration (30s)</button>
                </div>
                <div class="setting-item input-group">
                    <label for="volumePerSecond" style="text-align: center;">Mililiters per Second</label>
                    <input class="volumePerSecond" type="number" id="volumePerSecond" min="0.1" max="50.0" step="0.1" value="1.0">
                </div>
                <div class="setting-item">
                    <button class="btn btn-primary" onclick="updateVolumePerSecond()">Update Setting</button>
                </div>
            </div>

            <div class="card-subheader">Kalkwasser Pump Calibration</div>
            <div class="settings-row">
                <div class="setting-item">
                    <button id="kalkCalibBtn" class="btn btn-secondary" onclick="startKalkCalibration()">Start Calibration (30s)</button>
                </div>
                <div class="setting-item input-group">
                    <label for="kalkMeasuredMl" style="text-align: center;">Volume measured (ml in 30s)</label>
                    <input type="number" id="kalkMeasuredMl" min="0.1" max="500" step="0.1" placeholder="15.3">
                </div>
                <div class="setting-item">
                    <button class="btn btn-primary" onclick="saveKalkFlowRate()">Save Result</button>
                </div>
            </div>

        </div>

        <!-- Footer -->
        <div class="footer-info">
            Top Off Water System • v2.1
        </div>
    </div>

    <script>

        // ============================================
        // SESSION EXPIRY HANDLING (for VPS proxy)
        // ============================================

        let sessionExpired = false;
        let pollingIntervals = [];

        function handleSessionExpired() {
            if (sessionExpired) return;
            sessionExpired = true;

            // Stop all polling intervals
            pollingIntervals.forEach(id => clearInterval(id));
            pollingIntervals = [];
            if (monostableTimer) {
                clearTimeout(monostableTimer);
                monostableTimer = null;
            }

            // Show overlay
            const overlay = document.createElement('div');
            overlay.innerHTML = `
                <div style="
                    position: fixed; top: 0; left: 0; right: 0; bottom: 0;
                    background: rgba(0,0,0,0.85); z-index: 9999;
                    display: flex; justify-content: center; align-items: center;
                ">
                    <div style="
                        background: white; padding: 40px; border-radius: 12px;
                        text-align: center; max-width: 400px; margin: 20px;
                    ">
                        <h2 style="color: #e74c3c; margin-bottom: 15px;">Session Expired</h2>
                        <p style="color: #666; margin-bottom: 25px;">Your session has expired. Please log in again.</p>
                        <a href="login"
                           onclick="if(window.location.pathname.indexOf('/device/')!==-1){window.location.href='/dashboard';return false;}"
                           style="
                            display: inline-block; padding: 12px 30px;
                            background: #3498db; color: white;
                            text-decoration: none; border-radius: 8px;
                            font-weight: bold;
                        ">Back to Login</a>
                    </div>
                </div>
            `;
            document.body.appendChild(overlay);
            console.log("Session expired - polling stopped");
        }

        async function secureFetch(url, options = {}) {
            if (sessionExpired) return null;

            try {
                const response = await fetch(url, options);
                
                // Check for auth failure (401 or redirect to login)
                if (response.status === 401 || 
                    response.redirected && response.url.includes('/login')) {
                    handleSessionExpired();
                    return null;
                }
                return response;
            } catch (error) {
                console.error('Fetch error:', error);
                return null;
            }
        }


        // ============================================
        // STATE TRACKING
        // ============================================
        let systemEnabled = true;
        let maxDailyVolume = 2000;

        // Direct pump button state
        var pumpBtnDownTime = 0;
        var pumpBtnIsDown = false;
        var monostableTimer = null;
        var monostableActive = false;

        // ============================================
        // SYSTEM TOGGLE (bistable ON/OFF)
        // ============================================
        function toggleSystem() {
            const btn = document.getElementById("systemToggleBtn");
            btn.disabled = true;

            secureFetch("api/system-toggle", { method: "POST" })
                .then((response) => {
                    if (!response) { btn.disabled = false; return; }
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) {
                        systemEnabled = data.enabled;
                        updateSystemToggleButton(data.enabled);
                    }
                    btn.disabled = false;
                })
                .catch((error) => {
                    console.error("Toggle system error:", error);
                    btn.disabled = false;
                });
        }

        function updateSystemToggleButton(enabled) {
            const btn = document.getElementById("systemToggleBtn");
            if (!btn) return;

            if (enabled) {
                btn.textContent = "System On";
                btn.className = "btn btn-primary";
            } else {
                btn.textContent = "System Off";
                btn.className = "btn btn-kalk-off";
            }
        }

        function loadSystemState() {
            secureFetch("api/system-toggle")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    if (data.success) {
                        systemEnabled = data.enabled;
                        updateSystemToggleButton(data.enabled);
                    }
                })
                .catch((error) => console.error("Failed to load system state:", error));
        }

        // ============================================
        // SYSTEM RESET (monostable)
        // ============================================
        function systemReset() {
            var btn = document.getElementById("systemResetBtn");
            btn.disabled = true;
            btn.textContent = "Resetting...";
            fetch("api/system-reset", { method: "POST" })
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (!data.success) console.error("System reset failed:", data.message);
                })
                .catch(function(e) { console.error("System reset error:", e); })
                .finally(function() {
                    btn.disabled = false;
                    btn.textContent = "System Reset";
                });
        }

        // ============================================
        // MANUAL PUMP (bistable + monostable)
        // ============================================
        (function initPumpBtn() {
            var btn = document.getElementById("manualPumpBtn");
            if (!btn) return;
            btn.addEventListener("mousedown", onPumpBtnDown);
            btn.addEventListener("mouseup", onPumpBtnUp);
            btn.addEventListener("mouseleave", onPumpBtnUp);
            btn.addEventListener("touchstart", function(e) { e.preventDefault(); onPumpBtnDown(); });
            btn.addEventListener("touchend", function(e) { e.preventDefault(); onPumpBtnUp(); });
        })();

        function onPumpBtnDown() {
            pumpBtnIsDown = true;
            pumpBtnDownTime = Date.now();
            monostableTimer = setTimeout(function() {
                monostableActive = true;
                fetch("api/pump/direct-on", {
                    method: "POST",
                    headers: {"Content-Type": "application/x-www-form-urlencoded"},
                    body: "mode=monostable"
                })
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (data.success) {
                        updatePumpButton(true);
                    } else {
                        monostableActive = false;
                        console.error("Failed to start pump (monostable)");
                    }
                })
                .catch(function(e) { monostableActive = false; console.error("Pump error:", e); });
            }, 3000);
        }

        function onPumpBtnUp() {
            if (!pumpBtnIsDown) return;
            pumpBtnIsDown = false;
            var holdDuration = Date.now() - pumpBtnDownTime;
            if (monostableTimer) {
                clearTimeout(monostableTimer);
                monostableTimer = null;
            }
            if (monostableActive) {
                monostableActive = false;
                fetch("api/pump/direct-off", { method: "POST" })
                    .then(function(r) { return r.json(); })
                    .then(function(data) { if (data.success) updatePumpButton(false); })
                    .catch(function(e) { console.error("Pump stop error:", e); });
            } else if (holdDuration < 3000) {
                var pumpIsOn = document.getElementById("manualPumpBtn").classList.contains("btn-primary");
                if (pumpIsOn) {
                    fetch("api/pump/direct-off", { method: "POST" })
                        .then(function(r) { return r.json(); })
                        .then(function(data) { if (data.success) updatePumpButton(false); })
                        .catch(function(e) { console.error("Pump stop error:", e); });
                } else {
                    fetch("api/pump/direct-on", { method: "POST" })
                        .then(function(r) { return r.json(); })
                        .then(function(data) {
                            if (data.success) updatePumpButton(true);
                            else console.error("Failed to start pump");
                        })
                        .catch(function(e) { console.error("Pump start error:", e); });
                }
            }
        }

        function updatePumpButton(isOn) {
            var btn = document.getElementById("manualPumpBtn");
            if (!btn) return;
            if (isOn) {
                btn.textContent = "Manual Pump ON";
                btn.className = "btn btn-primary";
            } else {
                btn.textContent = "Manual Pump OFF";
                btn.className = "btn btn-off";
            }
        }

        // ============================================
        // MIXING PUMP DIRECT (bistable)
        // ============================================
        function toggleMixingPump() {
            const btn = document.getElementById("mixingPumpBtn");
            const isOn = btn.classList.contains("btn-primary");
            btn.disabled = true;
            secureFetch("api/mixing-pump", {
                method: "POST",
                headers: {"Content-Type": "application/x-www-form-urlencoded"},
                body: "action=" + (isOn ? "off" : "on")
            })
            .then(function(r) { if (!r) return null; return r.json(); })
            .then(function(data) {
                if (data && data.success) updateMixingPumpButton(data.active);
                else if (data) console.error("Mixing pump error:", data.error);
                btn.disabled = false;
            })
            .catch(function(e) { console.error("Mixing pump error:", e); btn.disabled = false; });
        }

        function updateMixingPumpButton(isOn) {
            var btn = document.getElementById("mixingPumpBtn");
            if (!btn) return;
            btn.textContent = isOn ? "Mixing Pump ON" : "Mixing Pump OFF";
            btn.className   = "btn " + (isOn ? "btn-primary" : "btn-off");
        }

        // ============================================
        // PERISTALTIC PUMP DIRECT (bistable)
        // ============================================
        function togglePeristalticPump() {
            const btn = document.getElementById("peristalticPumpBtn");
            const isOn = btn.classList.contains("btn-primary");
            btn.disabled = true;
            secureFetch("api/peristaltic-pump", {
                method: "POST",
                headers: {"Content-Type": "application/x-www-form-urlencoded"},
                body: "action=" + (isOn ? "off" : "on")
            })
            .then(function(r) { if (!r) return null; return r.json(); })
            .then(function(data) {
                if (data && data.success) updatePeristalticPumpButton(data.active);
                else if (data) console.error("Peristaltic pump error:", data.error);
                btn.disabled = false;
            })
            .catch(function(e) { console.error("Peristaltic pump error:", e); btn.disabled = false; });
        }

        function updatePeristalticPumpButton(isOn) {
            var btn = document.getElementById("peristalticPumpBtn");
            if (!btn) return;
            btn.textContent = isOn ? "Peristaltic ON" : "Peristaltic OFF";
            btn.className   = "btn " + (isOn ? "btn-primary" : "btn-off");
        }

        // ============================================
        // EXTENDED PUMP (Calibration) — uses direct pump
        // ============================================
        function triggerExtendedPump() {
            const btn = document.getElementById("extendedBtn");
            btn.disabled = true;
            btn.textContent = "Starting...";

            fetch("api/pump/direct-on", { method: "POST" })
                .then((response) => response.json())
                .then((data) => {
                    if (!data.success) console.error("Failed to start calibration pump");
                })
                .catch((e) => console.error("Calibration pump error:", e))
                .finally(() => {
                    btn.disabled = false;
                    btn.textContent = "Start Calibration";
                });
        }

        // ============================================
        // VOLUME SETTINGS
        // ============================================
        function loadVolumePerSecond() {
            fetch("api/pump-settings")
                .then((response) => response.json())
                .then((data) => {
                    if (data.success) {
                        document.getElementById("volumePerSecond").value = parseFloat(data.volume_per_second).toFixed(1);
                    }
                })
                .catch((error) => {
                    console.error("Failed to load volume setting:", error);
                });
        }

        function updateVolumePerSecond() {
            const volumeInput = document.getElementById("volumePerSecond");
            const volumeValue = parseFloat(volumeInput.value);

            if (volumeValue < 0.1 || volumeValue > 50.0) return;

            const formData = new FormData();
            formData.append("volume_per_second", volumeValue);

            fetch("api/pump-settings", { method: "POST", body: formData })
                .then((response) => response.json())
                .then((data) => {
                    if (data.success) {
                        volumeInput.value = volumeValue.toFixed(1);
                    }
                })
                .catch((error) => {
                    statusSpan.textContent = "Network error";
                });
        }

        // ============================================
        // STATUS BADGE UPDATES
        // ============================================
        function updateSensorBadge(badgeId, isActive) {
            const el = document.getElementById(badgeId);
            if (!el) return;
            el.textContent = 'Sensors: ' + (isActive ? 'ON' : 'OFF');
            el.className = isActive ? 'sub-on' : 'sub-off';
        }

        function updatePumpBadge(badgeId, isActive, attempt) {
            const el = document.getElementById(badgeId);
            if (!el) return;
            if (!isActive) {
                el.textContent = 'Pump: IDLE'; el.className = 'sub-off';
            } else {
                el.textContent = 'Pump: ACTIVE';
                el.className = attempt >= 3 ? 'sub-danger' : attempt === 2 ? 'sub-warn' : 'sub-on';
            }
        }

        function updateSystemBadge(badgeId, hasError, isDisabled, hasWarning) {
            const main = document.getElementById('statusMain');
            if (!main) return;
            main.className = 'status-main';
            if (hasError)         main.classList.add('status-error');
            else if (isDisabled)  main.classList.add('status-disabled');
            else if (hasWarning)  main.classList.add('status-warn');
            else                  main.classList.add('status-ok');
        }

        function formatTime(seconds) {
            if (!seconds || seconds <= 0) return "—";
            if (seconds < 60) return seconds + " sec";
            const minutes = Math.floor(seconds / 60);
            const secs = seconds % 60;
            if (secs === 0) return minutes + " min";
            return minutes + "m " + secs + "s";
        }

        function formatUptime(milliseconds) {
            const seconds = Math.floor(milliseconds / 1000);
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            return hours + "h " + minutes + "m";
        }

        // ============================================
        // MAIN STATUS UPDATE
        // ============================================
        function updateStatus() {
            secureFetch("api/status")
                .then((response) => {
                    if (!response) return null;
                    return response.json();
                })
                .then((data) => {
                    if (!data) return;
                    
                    // Low reservoir alarm badges
                    const hasLowResError   = !!data.low_reservoir_error;
                    const hasLowResWarning = !!data.low_reservoir_warning && !hasLowResError;
                    const resCount         = data.low_reservoir_count || 0;
                    const reservoirSep   = document.getElementById("reservoirSep");
                    const reservoirBadge = document.getElementById("reservoirAlarmBadge");
                    if (hasLowResError) {
                        reservoirSep.style.display   = '';
                        reservoirBadge.style.display = '';
                        reservoirBadge.className     = 'sub-danger';
                        reservoirBadge.textContent   = '\u26a0 Reservoir empty \u2014 refill & reset';
                    } else if (hasLowResWarning) {
                        reservoirSep.style.display   = '';
                        reservoirBadge.style.display = '';
                        reservoirBadge.className     = 'sub-warn';
                        reservoirBadge.textContent   = '\u26a0 Low water reservoir (' + resCount + '/3)';
                    } else {
                        reservoirSep.style.display   = 'none';
                        reservoirBadge.style.display = 'none';
                    }

                    // Badges
                    updateSensorBadge("sensor1Badge", data.sensor_active);
                    updateSensorBadge("sensor2Badge", data.sensor_active);
                    updatePumpBadge("pumpBadge", data.pump_active, data.pump_attempt || 0);
                    updateSystemBadge("systemBadge", data.system_error, data.system_disabled, hasLowResWarning);

                    // Process status
                    document.getElementById("processDescription").textContent = data.state_description || "IDLE - Waiting for sensors";

                    // System toggle sync
                    if (typeof data.system_disabled !== 'undefined') {
                        systemEnabled = !data.system_disabled;
                        updateSystemToggleButton(!data.system_disabled);
                    }

                    // Sync manual pump button with actual pump state
                    updatePumpButton(data.pump_active);
                    updateMixingPumpButton(data.mixing_pump_active || false);
                    updatePeristalticPumpButton(data.peristaltic_pump_active || false);

                    // WiFi status
                    const wifiItem = document.getElementById("wifiItem");
                    wifiItem.className = 'status-main-wifi ' + (data.wifi_connected ? 'wifi-on' : 'wifi-off');

                    // RTC
                    const rtcItem = document.getElementById("rtcItem");
                    const rtcTime = document.getElementById("rtcTime");
                    const rtcHint = document.getElementById("rtcHint");
                    rtcTime.textContent = data.rtc_time || "Error";
                    if (data.rtc_battery_issue || data.rtc_needs_sync) {
                        rtcItem.className = "status-info rtc-warn";
                        rtcHint.textContent = " ⚠ Battery";
                    } else {
                        rtcItem.className = "status-info";
                        rtcHint.textContent = "";
                    }

                    // Memory & Uptime
                    document.getElementById("freeHeap").textContent = (data.free_heap / 1024).toFixed(1) + " KB";
                    document.getElementById("uptime").textContent = formatUptime(data.uptime);

                    // Note: manualPumpBtn is NOT disabled when system_disabled — direct pump bypasses system state

                    const extendedBtn = document.getElementById("extendedBtn");
                    if (extendedBtn) {
                        extendedBtn.disabled = data.pump_active;
                    }

                    // Kalkwasser schedule update
                    if (data.rtc_ts) {
                        updateKalkSchedule(data.rtc_ts,
                                           data.kalk_mix_done_bits  || 0,
                                           data.kalk_dose_done_bits || 0,
                                           data.kalk_state || 'IDLE',
                                           data.kalk_enabled || false,
                                           data.kalk_alarm || false);
                    }
                })
                .catch((error) => {
                    console.error("Status update failed:", error);
                });
        }


        // ============================================
        // DAILY VOLUME + AVAILABLE WATER + DOSE DISPLAY
        // Wszystkie dane z jednego endpointu: api/daily-volume
        // ============================================
        function loadDailyVolume() {
            secureFetch("api/daily-volume")
                .then((r) => { if (!r) return null; return r.json(); })
                .then((data) => {
                    if (!data || !data.success) return;

                    const rolling  = data.rolling_24h_ml;
                    const limit    = data.daily_limit_ml;
                    const dose     = data.dose_ml;

                    // Daily Refill Limit bar
                    const pctUsed = limit > 0 ? Math.min((rolling / limit) * 100, 100) : 0;
                    document.getElementById("volumeBarFill").style.width = pctUsed + "%";
                    document.getElementById("volumeText").textContent = rolling + " ml / " + limit + " ml";

                    // Single Dose bar (dose jako % max 2000 ml)
                    const pctDose = Math.min((dose / 2000) * 100, 100);
                    document.getElementById("doseBarFill").style.width = pctDose + "%";
                    document.getElementById("doseText").textContent = dose + " ml / cycle";

                    // Populate inputs with current saved values (skip if user is editing)
                    const limitInp = document.getElementById("dailyLimitInput");
                    if (document.activeElement !== limitInp) limitInp.value = limit;
                    const doseInp = document.getElementById("doseInput");
                    if (document.activeElement !== doseInp) doseInp.value = dose;
                })
                .catch((e) => console.error("loadDailyVolume error:", e));
        }

        // ============================================
        // SET DAILY LIMIT
        // ============================================
        function setDailyLimit() {
            const input = document.getElementById("dailyLimitInput");
            const value = parseInt(input.value);
            if (isNaN(value) || value < 100 || value > 10000) return;
            if (!confirm("Set daily refill limit to " + value + " ml?")) return;

            secureFetch("api/set-fill-water-max", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "value=" + value
            })
                .then((r) => { if (!r) return null; return r.json(); })
                .then((data) => {
                    if (!data) return;
                    if (data.success) { loadDailyVolume(); }
                    else console.error("Set daily limit failed:", data.error);
                })
                .catch((e) => console.error("Set daily limit error:", e));
        }


        // ============================================
        // SET SINGLE DOSE
        // ============================================
        function setDose() {
            const input = document.getElementById("doseInput");
            const value = parseInt(input.value);
            if (isNaN(value) || value < 50 || value > 2000) return;
            if (!confirm("Set single dose to " + value + " ml?")) return;

            secureFetch("api/set-dose", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "value=" + value
            })
                .then((r) => { if (!r) return null; return r.json(); })
                .then((data) => {
                    if (!data) return;
                    if (data.success) { loadDailyVolume(); }
                    else console.error("Set dose failed:", data.error);
                })
                .catch((e) => console.error("Set dose error:", e));
        }

        // ============================================
        // LOGOUT
        // ============================================
        function logout() {
            fetch("api/logout", { method: "POST" }).then(() => {
                if (window.location.pathname.indexOf("/device/") !== -1) {
                    window.location.href = "/dashboard";
                } else {
                    window.location.href = "login";
                }
            });
        }

        // ============================================
        // CYCLE CHARTS
        // ============================================
        var CC = {
            bg:'#1e293b', grid:'rgba(148,163,184,0.10)', txt:'#94a3b8',
            ema:'#22d3ee', ok:'#4ade80', warn:'#fbbf24', err:'#f87171',
            boot:'#64748b',
            bandG:'rgba(74,222,128,0.13)',   // green  — normal range (±yellow_sigma)
            bandY:'rgba(251,191,36,0.13)',   // yellow — warning ring (between yellow and red)
            bandR:'rgba(239,68,68,0.12)'     // red    — alarm zone (outside ±red_sigma)
        };


        function fmtV(v) { return Math.round(v); }

        function drawTimeAxisScrollable(ctx, minTs, maxTs, W, H, p) {
            var DAY_S = 86400;
            var tsRange = Math.max(maxTs - minTs, 1);
            var cw = W - p.l - p.r;
            function xTs(ts) { return p.l + (ts - minTs) / tsRange * cw; }

            // Alternating day background bands (every other day slightly lighter)
            var firstDay = Math.floor(minTs / DAY_S) * DAY_S;
            for (var d = firstDay, idx = 0; d < maxTs; d += DAY_S, idx++) {
                if (idx % 2 === 0) continue;
                var x1 = Math.max(p.l, xTs(d));
                var x2 = Math.min(p.l + cw, xTs(d + DAY_S));
                if (x2 > x1) {
                    ctx.fillStyle = 'rgba(255,255,255,0.025)';
                    ctx.fillRect(x1, p.t, x2 - x1, H - p.t - p.b);
                }
            }

            // Hour tick labels (HH:MM) — always 1h intervals since view = 6h
            var firstTick = Math.ceil(minTs / 3600) * 3600;
            ctx.font = '10px sans-serif'; ctx.textAlign = 'center';
            for (var ts = firstTick; ts <= maxTs; ts += 3600) {
                var d = new Date(ts * 1000);
                ctx.fillStyle = '#94a3b8';
                ctx.fillText(
                    String(d.getHours()).padStart(2,'0') + ':' + String(d.getMinutes()).padStart(2,'0'),
                    xTs(ts), H - p.b + 13
                );
            }

            // Midnight separators + day date labels (DD.MM)
            var firstMid = Math.ceil(minTs / DAY_S) * DAY_S;
            for (var ts = firstMid; ts <= maxTs; ts += DAY_S) {
                var x = xTs(ts);
                ctx.save();
                ctx.strokeStyle = 'rgba(148,163,184,0.18)';
                ctx.lineWidth = 1; ctx.setLineDash([3, 4]);
                ctx.beginPath(); ctx.moveTo(x, p.t); ctx.lineTo(x, H - p.b); ctx.stroke();
                ctx.setLineDash([]); ctx.restore();
                var d = new Date(ts * 1000);
                var lbl = String(d.getDate()).padStart(2,'0') + '.' + String(d.getMonth()+1).padStart(2,'0');
                ctx.fillStyle = '#f97316'; ctx.font = '10px sans-serif'; ctx.textAlign = 'center';
                ctx.fillText(lbl, x, H - p.b + 26);
            }

            // Left-edge date label for the starting day (when it's not at midnight)
            var d0 = new Date(minTs * 1000);
            if (d0.getHours() !== 0 || d0.getMinutes() !== 0) {
                var lbl0 = String(d0.getDate()).padStart(2,'0') + '.' + String(d0.getMonth()+1).padStart(2,'0');
                ctx.fillStyle = 'rgba(249,115,22,0.6)'; ctx.font = '10px sans-serif'; ctx.textAlign = 'left';
                ctx.fillText(lbl0, p.l + 3, H - p.b + 26);
            }
        }

        function drawRateChart(pts, ema) {
            var VIEW_HOURS = 6;
            var MAX_SPAN_S = 5 * 86400;
            var scrollEl = document.getElementById('chartRateScroll');
            var viewW = scrollEl.offsetWidth;
            var pxPerHour = viewW / VIEW_HOURS;

            // Determine time range: min 6h, max 5 days, anchored to newest point
            var maxTs = pts.length ? pts[pts.length-1].ts : Math.floor(Date.now()/1000);
            var minTs = pts.length ? pts[0].ts : maxTs - VIEW_HOURS * 3600;
            var spanS = maxTs - minTs;
            if (spanS < VIEW_HOURS * 3600) { minTs = maxTs - VIEW_HOURS * 3600; spanS = VIEW_HOURS * 3600; }
            if (spanS > MAX_SPAN_S)        { minTs = maxTs - MAX_SPAN_S;        spanS = MAX_SPAN_S; }

            var canvasW = Math.round(pxPerHour * spanS / 3600);
            var H = 190;
            var cv = document.getElementById('chartRate');
            cv.style.width = canvasW + 'px';
            cv.style.height = H + 'px';
            var dpr = window.devicePixelRatio || 1;
            cv.width = Math.round(canvasW * dpr); cv.height = Math.round(H * dpr);
            var ctx = cv.getContext('2d');
            ctx.scale(dpr, dpr);
            var W = canvasW;

            var p = {t:14, r:8, b:44, l:8};  // b=44: 2-row axis (HH:MM + DD.MM)
            var cw = W-p.l-p.r, ch = H-p.t-p.b;
            ctx.fillStyle = CC.bg; ctx.fillRect(0,0,W,H);

            var tsRange = spanS;
            function xOf(ts) { return p.l + (ts - minTs) / tsRange * cw; }
            function yOf(v)  { return p.t + (1-(v-yMin)/(yMax-yMin)) * ch; }

            // Only points within the visible window
            var visPts = pts.filter(function(c) { return c.ts >= minTs; });

            var rates = visPts.filter(function(c){return c.rate_ml_h>0;}).map(function(c){return c.rate_ml_h;});
            var hasEma = ema.ema_dev > 0.01 && ema.bootstrap >= 3;
            var yMin=0, yMax=100;
            if (rates.length) { yMin=Math.min.apply(null,rates); yMax=Math.max.apply(null,rates); }
            if (hasEma) {
                var rDev0=ema.red_sigma/100*ema.ema_dev;
                yMin=Math.min(yMin, Math.max(0,ema.ema_rate-rDev0));
                yMax=Math.max(yMax, ema.ema_rate+rDev0);
            }
            var rpad=(yMax-yMin)*0.18||yMax*0.18||5;
            yMin=Math.max(0,yMin-rpad); yMax+=rpad;

            // Faint horizontal grid lines
            ctx.strokeStyle=CC.grid; ctx.lineWidth=0.5;
            for (var gi=0; gi<=4; gi++){
                var gy=p.t+ch*gi/4;
                ctx.beginPath(); ctx.moveTo(p.l,gy); ctx.lineTo(p.l+cw,gy); ctx.stroke();
            }

            if (hasEma) {
                var yD=ema.yellow_sigma/100*ema.ema_dev;
                var rD=ema.red_sigma/100*ema.ema_dev;
                var yRt=yOf(ema.ema_rate+rD), yRb=yOf(Math.max(yMin,ema.ema_rate-rD));
                var yYt=yOf(ema.ema_rate+yD), yYb=yOf(Math.max(yMin,ema.ema_rate-yD));

                ctx.fillStyle=CC.bandR;
                ctx.fillRect(p.l, p.t,  cw, yRt-p.t);
                ctx.fillRect(p.l, yRb,  cw, p.t+ch-yRb);
                ctx.fillStyle=CC.bandY;
                ctx.fillRect(p.l, yRt,  cw, yYt-yRt);
                ctx.fillRect(p.l, yYb,  cw, yRb-yYb);
                ctx.fillStyle=CC.bandG;
                ctx.fillRect(p.l, yYt,  cw, yYb-yYt);

                ctx.font='bold 10px sans-serif'; ctx.textAlign='left';
                function drawBoundLabel(yPos, val) {
                    if (yPos < p.t || yPos > p.t+ch) return;
                    ctx.fillStyle='#94a3b8';
                    ctx.fillText(fmtV(val), p.l+4, yPos+3);
                }
                drawBoundLabel(yRt, ema.ema_rate+rD);
                drawBoundLabel(yRb, Math.max(0, ema.ema_rate-rD));
                drawBoundLabel(yYt, ema.ema_rate+yD);
                drawBoundLabel(yYb, Math.max(0, ema.ema_rate-yD));

                var ye = yOf(ema.ema_rate);
                var emaLbl = 'EMA ' + fmtV(ema.ema_rate);
                ctx.font='bold 10px sans-serif';
                var lblW = ctx.measureText(emaLbl).width + 6;
                ctx.strokeStyle='#94a3b8'; ctx.lineWidth=1.5; ctx.setLineDash([6,4]);
                ctx.beginPath(); ctx.moveTo(p.l+lblW, ye); ctx.lineTo(p.l+cw, ye); ctx.stroke();
                ctx.setLineDash([]);
                ctx.fillStyle='#94a3b8'; ctx.textAlign='left';
                ctx.fillText(emaLbl, p.l+4, ye+3);
            }

            // Data line
            ctx.strokeStyle='rgba(148,163,184,0.28)'; ctx.lineWidth=1.5;
            ctx.beginPath(); var first=true;
            for (var i=0; i<visPts.length; i++) {
                if (visPts[i].rate_ml_h<=0) continue;
                var x=xOf(visPts[i].ts), y=yOf(visPts[i].rate_ml_h);
                if (first){ctx.moveTo(x,y);first=false;}else ctx.lineTo(x,y);
            }
            ctx.stroke();

            // Data points
            for (var i=0; i<visPts.length; i++) {
                if (visPts[i].rate_ml_h<=0) continue;
                ctx.beginPath(); ctx.arc(xOf(visPts[i].ts),yOf(visPts[i].rate_ml_h),4,0,Math.PI*2);
                ctx.fillStyle='rgba(100,116,139,0.85)'; ctx.fill();
            }

            drawTimeAxisScrollable(ctx, minTs, maxTs, W, H, p);

            if (!visPts.length) {
                ctx.fillStyle=CC.txt; ctx.font='13px sans-serif'; ctx.textAlign='center';
                ctx.fillText('No data yet', W/2, H/2);
            }

            // Jump to latest data (right edge) on load
            scrollEl.scrollLeft = scrollEl.scrollWidth;
        }

        function scrollRateChart(dir) {
            var el = document.getElementById('chartRateScroll');
            if (!el) return;
            el.scrollTo({ left: el.scrollLeft + dir * el.offsetWidth * 0.5, behavior: 'smooth' });
        }

        function deleteCycleHistory() {
            if (!confirm("Delete all cycle history and EMA data?\nThis cannot be undone.")) return;
            var btn = document.getElementById("deleteCyclesBtn");
            btn.disabled = true; btn.textContent = "Deleting...";
            secureFetch("api/clear-cycle-history", { method: "POST" })
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (data && data.success) {
                        var cv = document.getElementById('chartRate');
                        if (cv) { var ctx = cv.getContext('2d'); ctx.clearRect(0,0,cv.width,cv.height); }
                    } else {
                        alert("Delete failed: " + (data && data.error ? data.error : "unknown error"));
                    }
                })
                .catch(function(e) { console.error("Delete cycles error:", e); })
                .finally(function() { btn.disabled=false; btn.textContent="Delete Data"; });
        }

        function loadCycleHistory() {
            var btn = document.getElementById("loadCyclesBtn");
            btn.disabled = true; btn.textContent = "Loading...";

            secureFetch("api/cycle-history")
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (!data || !data.success) return;
                    var pts = (data.cycles || []).slice().reverse(); // oldest first
                    var ema = {
                        ema_rate: data.ema_rate || 0, ema_dev: data.ema_dev || 0,
                        bootstrap: data.bootstrap || 0,
                        yellow_sigma: data.yellow_sigma || 150, red_sigma: data.red_sigma || 250
                    };
                    drawRateChart(pts, ema);
                })
                .catch(function(e) { console.error("Load cycles error:", e); })
                .finally(function() { btn.disabled=false; btn.textContent="Load History"; });
        }

        // ============================================
        // KALKWASSER
        // ============================================

        var KALK_MIX_BASES  = [0, 6, 12, 18];
        var KALK_DOSE_HOURS = [2,3,4,5,8,9,10,11,14,15,16,17,20,21,22,23];

        function updateKalkSchedule(rtcTs, mixDoneBits, doseDoneBits, kalkState, kalkEnabled, kalkAlarm) {
            var wrap = document.getElementById('kalkScheduleWrap');
            if (!wrap) return;

            var banner = document.getElementById('kalkAlarmBanner');
            if (banner) banner.style.display = (kalkEnabled && kalkAlarm) ? '' : 'none';

            var now = new Date(rtcTs * 1000);
            var nowH = now.getHours();
            var nowTotalMin = nowH * 60 + now.getMinutes();

            function setTile(id, cls) {
                var el = document.getElementById(id);
                if (el) el.className = 'kalk-tile kalk-ev-' + cls;
            }

            // Mix slots: [0,6,12,18]:15
            // Bit i in mixDoneBits = slot i was executed today (set by firmware).
            var isMixState = (kalkState === 'MIXING' || kalkState === 'SETTLING' ||
                              kalkState === 'WAIT_MIX');
            KALK_MIX_BASES.forEach(function(base, i) {
                var slotMin   = base * 60 + 15;
                var isNowSlot = (nowTotalMin >= slotMin && nowTotalMin < slotMin + 360);
                var slotDone  = !!(mixDoneBits & (1 << i));
                var cls;
                if      (isMixState && isNowSlot) cls = 'active';   // yellow — mixing/settling now
                else if (slotDone)                cls = 'done';     // green — confirmed executed today
                else if (slotMin > nowTotalMin)   cls = 'pending';  // white — future
                else                              cls = 'missed';   // red — past, not executed
                setTile('kalkMix' + i, cls);
            });

            // Dose slots: 02-05, 08-11, 14-17, 20-23
            // Bit i in doseDoneBits = slot i was executed today (set by firmware).
            var isDosing = (kalkState === 'DOSING' || kalkState === 'WAIT_DOSE');
            KALK_DOSE_HOURS.forEach(function(h, i) {
                var slotDone = !!(doseDoneBits & (1 << i));
                var cls;
                if      (isDosing && nowH === h) cls = 'active';   // yellow — dosing now
                else if (slotDone)               cls = 'done';     // green — confirmed executed today
                else if (h > nowH)               cls = 'pending';  // white — future
                else                             cls = 'missed';   // red — past, not executed
                setTile('kalkDose' + i, cls);
            });
        }

        function startKalkCalibration() {
            var btn = document.getElementById('kalkCalibBtn');
            btn.disabled = true; btn.textContent = 'Running 30s...';
            secureFetch('api/kalkwasser-calibrate', { method: 'POST' })
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (data && data.success) {
                        btn.textContent = 'Done! Measure now.';
                    } else {
                        alert('Calibration failed: ' + (data && data.error ? data.error : 'pump busy'));
                        btn.disabled = false; btn.textContent = 'Start Calibration (30s)';
                    }
                })
                .catch(function() { btn.disabled = false; btn.textContent = 'Start Calibration (30s)'; });
            setTimeout(function() {
                btn.disabled = false; btn.textContent = 'Start Calibration (30s)';
            }, 35000);
        }

        function saveKalkFlowRate() {
            var ml = parseFloat(document.getElementById('kalkMeasuredMl').value);
            if (!ml || ml <= 0) { alert('Enter measured volume in ml'); return; }
            secureFetch('api/kalkwasser-flow-rate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'measured_ml=' + ml
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(data) {
                if (data && data.success) {
                    var fr = data.flow_rate_ul_s || 0;
                    if (fr > 0) {
                        document.getElementById('kalkMeasuredMl').placeholder = (fr / 1000 * 30).toFixed(1);
                    }
                }
            });
        }

        var kalkIsEnabled = false;

        function updateKalkButton(enabled) {
            kalkIsEnabled = enabled;
            var btn = document.getElementById('kalkwasserBtn');
            if (!btn) return;
            if (enabled) {
                btn.className = 'btn btn-kalk-on';
                btn.textContent = 'Kalkwasser ON';
            } else {
                btn.className = 'btn btn-kalk-off';
                btn.textContent = 'Kalkwasser OFF';
            }
        }

        function toggleKalkwasser() {
            secureFetch('api/kalkwasser-config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'enabled=' + (kalkIsEnabled ? 0 : 1)
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(data) {
                if (data && data.success) updateKalkButton(!!data.enabled);
            });
        }

        function saveKalkConfig() {
            var dose = parseInt(document.getElementById('kalkDailyDose').value) || 0;
            secureFetch('api/kalkwasser-config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'enabled=' + (kalkIsEnabled ? 1 : 0) + '&daily_dose_ml=' + dose
            })
            .then(function(r) { return r ? r.json() : null; })
            .then(function(data) {
                if (data && data.success) {
                    var bar = document.getElementById('kalkDoseBarFill');
                    var txt = document.getElementById('kalkDoseBarText');
                    if (bar) bar.style.width = Math.min((dose / 500) * 100, 100) + '%';
                    if (txt) txt.textContent = dose > 0 ? dose + ' ml / day' : 'not configured';
                }
            });
        }

        function loadKalkConfig() {
            secureFetch('api/kalkwasser-config')
                .then(function(r) { return r ? r.json() : null; })
                .then(function(data) {
                    if (!data || !data.success) return;
                    updateKalkButton(!!data.enabled);
                    var dose = data.daily_dose_ml || 0;
                    var inp = document.getElementById('kalkDailyDose');
                    if (inp && document.activeElement !== inp) inp.value = dose;
                    var fr = data.flow_rate_ul_per_s || 0;
                    if (fr > 0) {
                        document.getElementById('kalkMeasuredMl').placeholder = (fr / 1000 * 30).toFixed(1);
                    }
                    // Aktualizuj pasek w 3. karcie
                    var bar = document.getElementById('kalkDoseBarFill');
                    var txt = document.getElementById('kalkDoseBarText');
                    if (bar) bar.style.width = Math.min((dose / 500) * 100, 100) + '%';
                    if (txt) txt.textContent = dose > 0 ? dose + ' ml / day' : 'not configured';
                });
        }

        // ============================================
        // INITIALIZATION
        // ============================================

        // Register all intervals for cleanup on session expiry
        pollingIntervals.push(setInterval(updateStatus, 2000));
        pollingIntervals.push(setInterval(loadSystemState, 30000));
        pollingIntervals.push(setInterval(loadDailyVolume, 10000));
        pollingIntervals.push(setInterval(loadKalkConfig, 30000));

        // Initial loads
        updateStatus();
        loadSystemState();
        loadVolumePerSecond();
        loadDailyVolume();
        loadKalkConfig();
    </script>
</body>
</html>
)rawliteral";

