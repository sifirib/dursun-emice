from fastapi import FastAPI, File, HTTPException, UploadFile
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

    try:

        audio_bytes = await audio.read()

        text = await gemini.chat(
            audio_bytes=audio_bytes,
            mime_type=audio.content_type
        )

        mp3 = await tts.speak(text)

        return Response(
            content=mp3,
            media_type="audio/mpeg",
            headers={
                "Content-Disposition": 'inline; filename="response.mp3"'
            }
        )

    except Exception as e:

        raise HTTPException(
            status_code=502,
            detail=str(e)
        )