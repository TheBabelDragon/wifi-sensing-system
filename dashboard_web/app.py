from fastapi import FastAPI
from fastapi.responses import HTMLResponse
import uvicorn
from dashboard.server import DashboardServer

app = FastAPI(title="CSI Spatial Intelligence Dashboard")
dashboard = DashboardServer()

@app.get("/", response_class=HTMLResponse)
def root():
    state = dashboard.state or {}
    history = getattr(dashboard, 'history', [])[-6:]

    # Simple live voxel rendering (text-based for now)
    voxel_html = ""
    if 'voxels' in state:
        voxel_html = "<h3>Live Voxel Field</h3><pre style='font-size:0.75rem; line-height:1.1; background:#0f172a; padding:12px; border-radius:8px;'>"
        for row in state.get('voxels', [])[:12]:
            line = ''.join(['█' if v > 0.6 else ('▓' if v > 0.35 else ('▒' if v > 0.15 else '░')) for v in row[:20]])
            voxel_html += line + "\n"
        voxel_html += "</pre>"

    html = f"""
    <!DOCTYPE html>
    <html>
    <head>
        <title>CSI Dashboard</title>
        <meta http-equiv="refresh" content="2">
        <style>
            body {{ font-family: system-ui, sans-serif; background:#0f172a; color:#e2e8f0; padding:30px; max-width:1200px; margin:auto; }}
            .header {{ display:flex; justify-content:space-between; align-items:center; margin-bottom:25px; }}
            h1 {{ color:#60a5fa; margin:0; }}
            .card {{ background:#1e2937; border-radius:16px; padding:24px; margin-bottom:24px; }}
            .grid {{ display:grid; grid-template-columns: 2fr 1fr; gap:24px; }}
            pre {{ background:#0f172a; padding:14px; border-radius:8px; font-size:0.85rem; line-height:1.05; }}
            .metric {{ font-size:1.05rem; margin:6px 0; }}
        </style>
    </head>
    <body>
        <div class="header">
            <h1>WiFi CSI Spatial Intelligence</h1>
            <div style="color:#64748b">Frame {state.get('frame', '-')} • Auto-refresh</div>
        </div>

        <div class="grid">
            <div class="card">
                <h2>Live State</h2>
                <div class="metric"><strong>Active Tracks:</strong> {state.get('tracks', 0)}</div>
                <div class="metric"><strong>Events:</strong> {', '.join(state.get('events', [])) or 'None'}</div>
                <div class="metric"><strong>Behaviors:</strong> {', '.join(state.get('behaviors', [])) or 'N/A'}</div>
                {voxel_html}
            </div>

            <div class="card">
                <h2>Recent Activity</h2>
                <pre>{history}</pre>
            </div>
        </div>

        <p style="color:#64748b; text-align:center; font-size:0.9rem">Feeding real-time context into aurora-swarm-btc</p>
    </body>
    </html>
    """
    return html

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="warning")