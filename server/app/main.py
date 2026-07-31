from fastapi import FastAPI, UploadFile, File
from fastapi.responses import Response

from app.services.gemini import GeminiService
from app.services.tts import TTSService

app = FastAPI()

gemini = GeminiService()
tts = TTSService()

@app.get("/")
async def home():
    return {
        "status": "ok"
    }


@app.post("/chat")
async def chat(audio: UploadFile = File(...)):
    audio_bytes = await audio.read()
    response = await gemini.chat(audio_bytes)

    if not response.success:
        return response

    audio = await tts.speak(response.text)

    return Response(
        content=audio,
        media_type="audio/mpeg"
    )