from fastapi import FastAPI
from fastapi.responses import HTMLResponse
import uvicorn
from dashboard.server import DashboardServer

app = FastAPI(title="CSI Spatial Intelligence Dashboard")
dashboard = DashboardServer()

@app.get("/", response_class=HTMLResponse)
def root():
    state = dashboard.state or {}
    history = dashboard.history[-8:] if hasattr(dashboard, 'history') else []

    tracks_html = ""
    if state.get('tracks'):
        tracks_html = "<h3>Active Tracks</h3><ul>" + "".join([f"<li>ID {t.get('id', '?')}: pos={t.get('centroid')}, speed={t.get('speed', 0):.2f}</li>" for t in state.get('tracks', [])]) + "</ul>"

    events_html = ""
    if state.get('events'):
        events_html = "<h3>Recent Events</h3><ul>" + "".join([f"<li>{e}</li>" for e in state.get('events', [])]) + "</ul>"

    html = f"""
    <!DOCTYPE html>
    <html>
    <head>
        <title>CSI Spatial Intelligence</title>
        <meta http-equiv="refresh" content="2">
        <style>
            body {{ font-family: system-ui, sans-serif; background: #0f172a; color: #e2e8f0; padding: 30px; max-width: 1100px; margin: auto; line-height: 1.5; }}
            .header {{ display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; }}
            .card {{ background: #1e2937; border-radius: 16px; padding: 24px; margin-bottom: 24px; box-shadow: 0 10px 15px -3px rgb(0 0 0 / 0.1); }}
            .grid {{ display: grid; grid-template-columns: 1fr 1fr; gap: 24px; }}
            h1 {{ color: #60a5fa; margin: 0; }}
            h2, h3 {{ color: #bae6fd; }}
            pre {{ background: #0f172a; padding: 16px; border-radius: 8px; overflow-x: auto; font-size: 0.9rem; }}
            ul {{ padding-left: 20px; }}
            .metric {{ font-size: 1.1rem; }}
        </style>
    </head>
    <body>
        <div class="header">
            <h1>WiFi CSI Spatial Intelligence</h1>
            <div style="color:#64748b">Auto-refresh • Frame {state.get('frame', '-')}</div>
        </div>

        <div class="grid">
            <div class="card">
                <h2>Current State</h2>
                <div class="metric"><strong>Active Tracks:</strong> {state.get('tracks', 0)}</div>
                <div class="metric"><strong>Events:</strong> {', '.join(state.get('events', [])) or 'None'}</div>
                <div class="metric"><strong>Behaviors:</strong> {', '.join(state.get('behaviors', [])) or 'N/A'}</div>
            </div>

            <div class="card">
                {tracks_html}
                {events_html}
            </div>
        </div>

        <div class="card">
            <h2>Recent History</h2>
            <pre>{history}</pre>
        </div>

        <p style="color:#64748b; text-align:center">Connected to aurora-swarm-btc • Real-time spatial awareness</p>
    </body>
    </html>
    """
    return html

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="warning")