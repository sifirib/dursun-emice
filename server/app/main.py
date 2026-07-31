from fastapi import FastAPI, UploadFile, File
from fastapi.responses import PlainTextResponse

from app.gemini import GeminiService

app = FastAPI()
gemini = GeminiService()

@app.post("/chat")
async def chat(audio: UploadFile = File(...)):
    audio_bytes = await audio.read()

    response = gemini.chat(audio_bytes)

    return PlainTextResponse(response)