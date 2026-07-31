from fastapi import FastAPI

app = FastAPI(
    title="Dursun Emice API",
    version="0.1.0"
)


@app.get("/")
async def home():
    return {
        "status": "ok",
        "message": "Dursun Emice API çalışıyor."
    }


@app.get("/health")
async def health():
    return {
        "status": "healthy"
    }