from fastapi import FastAPI, UploadFile, File
from fastapi.responses import PlainTextResponse

from server.app.services.gemini import GeminiService

app = FastAPI()

gemini = GeminiService()


@app.get("/")
async def home():
    return {
        "status": "ok"
    }


@app.post("/chat")
async def chat(audio: UploadFile = File(...)):
    audio_bytes = await audio.read()
    response = await gemini.chat(audio_bytes)

    return response