from fastapi import FastAPI, UploadFile, File
from fastapi.responses import PlainTextResponse

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


@app.post("/chat")
async def chat(audio: UploadFile = File(...)):
    return PlainTextResponse("Dursun Emice burada.")