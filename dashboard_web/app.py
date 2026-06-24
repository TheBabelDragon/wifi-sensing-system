from fastapi import FastAPI
from fastapi.responses import HTMLResponse
import uvicorn
from dashboard.server import DashboardServer

app = FastAPI(title="CSI Spatial Intelligence Dashboard")
dashboard = DashboardServer()

@app.get("/", response_class=HTMLResponse)
def root():
    state = dashboard.state
    html = f"""
    <html>
    <head>
        <title>CSI Spatial Intelligence Dashboard</title>
        <meta http-equiv="refresh" content="3">
        <style>
            body {{ font-family: system-ui; background: #0f172a; color: #e2e8f0; padding: 40px; max-width: 900px; margin: auto; }}
            .card {{ background: #1e2937; padding: 24px; border-radius: 12px; margin-bottom: 20px; }}
            pre {{ background: #0f172a; padding: 16px; border-radius: 8px; overflow-x: auto; }}
        </style>
    </head>
    <body>
        <h1>WiFi CSI Spatial Intelligence Dashboard</h1>
        <div class="card">
            <h2>Current State</h2>
            <pre>{state}</pre>
        </div>
        <p style="color:#64748b">Auto-refreshing every 3 seconds</p>
    </body>
    </html>
    """
    return html

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)