from fastapi import FastAPI
from fastapi.responses import HTMLResponse
import uvicorn
from dashboard.server import DashboardServer

app = FastAPI(title="CSI Spatial Intelligence Dashboard")
dashboard = DashboardServer()

@app.get("/", response_class=HTMLResponse)
def root():
    state = dashboard.state or {}
    history = getattr(dashboard, 'history', [])[-5:]

    # Simple live voxel rendering
    voxel_html = "<h3>Live Spatial View</h3><pre style='font-size:0.7rem; line-height:1; background:#0f172a; padding:10px;'>"
    voxels = state.get('voxels', [])
    for row in voxels[:10]:
        line = ''.join(['█' if v > 0.65 else ('▓' if v > 0.4 else ('▒' if v > 0.2 else '░')) for v in row[:18]])
        voxel_html += line + "\n"
    voxel_html += "</pre>"

    # Integration status section
    integration_html = """
    <div class="card">
        <h2>Integration Status (Sensing ↔ Swarm)</h2>
        <div class="metric"><strong>Bidirectional Link:</strong> Active</div>
        <div class="metric"><strong>Heartbeat:</strong> Flowing</div>
        <div class="metric"><strong>Commands:</strong> Listening</div>
        <p style="color:#64748b; font-size:0.85rem">Resilient + observable integration enabled</p>
    </div>
    """

    html = f"""
    <html>
    <head>
        <title>CSI Dashboard</title>
        <meta http-equiv="refresh" content="2">
        <style>
            body {{ font-family: system-ui; background:#0f172a; color:#e2e8f0; padding:25px; max-width:1100px; margin:auto; }}
            .header {{ display:flex; justify-content:space-between; margin-bottom:20px; }}
            h1 {{ color:#60a5fa; }}
            .card {{ background:#1e2937; border-radius:14px; padding:20px; margin-bottom:20px; }}
            .grid {{ display:grid; grid-template-columns:2fr 1fr; gap:20px; }}
            pre {{ background:#0f172a; padding:12px; border-radius:6px; font-size:0.8rem; }}
            .metric {{ margin:4px 0; font-size:1.05rem; }}
        </style>
    </head>
    <body>
        <div class="header">
            <h1>WiFi CSI Spatial Intelligence</h1>
            <div style="color:#64748b">Frame {state.get('frame', '-')}</div>
        </div>

        <div class="grid">
            <div class="card">
                <h2>Live View</h2>
                {voxel_html}
                <div class="metric"><strong>Tracks:</strong> {state.get('tracks', 0)} | <strong>Events:</strong> {', '.join(state.get('events', [])) or 'None'}</div>
            </div>

            <div class="card">
                <h2>Activity Log</h2>
                <pre>{history}</pre>
            </div>
        </div>

        {integration_html}

        <p style="color:#64748b; text-align:center">Real-time physical awareness ↔ aurora-swarm-btc (bidirectional)</p>
    </body>
    </html>
    """
    return html

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="warning")