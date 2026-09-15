import traceback

from fastapi import (
    FastAPI,
    File,
    HTTPException,
    Request,
    UploadFile,
)

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


@app.get("/health")
async def health():

    return {
        "status": "ok"
    }


async def process_audio(
    audio_bytes: bytes,
    mime_type: str
) -> Response:

    if not audio_bytes:
        raise RuntimeError(
            "Ses verisi bos."
        )

    print(
        f"[CHAT] Ses alindi: "
        f"{len(audio_bytes)} byte"
    )

    try:

        print(
            "[CHAT] Gemini basliyor..."
        )

        text = await gemini.chat(
            audio_bytes=audio_bytes,
            mime_type=mime_type
        )

        print(
            f"[CHAT] Gemini cevabi: {text}"
        )

    except Exception as exc:

        raise RuntimeError(
            f"GEMINI: {exc}"
        ) from exc

    try:

        print(
            "[CHAT] TTS basliyor..."
        )

        mp3 = await tts.speak(
            text
        )

        print(
            f"[CHAT] TTS MP3 boyutu: "
            f"{len(mp3)} byte"
        )

    except Exception as exc:

        raise RuntimeError(
            f"TTS: {exc}"
        ) from exc

    return Response(
        content=mp3,
        media_type="audio/mpeg",
        headers={
            "Content-Disposition":
                'inline; filename="response.mp3"'
        }
    )


@app.post("/chat")
async def chat(
    audio: UploadFile = File(...)
):

    try:

        audio_bytes = await audio.read()

        return await process_audio(
            audio_bytes=audio_bytes,
            mime_type=audio.content_type
        )

    except Exception as exc:

        print(
            "[CHAT] HATA:"
        )

        print(
            traceback.format_exc()
        )

        raise HTTPException(
            status_code=502,
            detail=str(exc)
        ) from exc


@app.post("/chat_raw")
async def chat_raw(
    request: Request
):

    try:

        audio_bytes = await request.body()

        return await process_audio(
            audio_bytes=audio_bytes,
            mime_type="audio/wav"
        )

    except Exception as exc:

        print(
            "[CHAT_RAW] HATA:"
        )

        print(
            traceback.format_exc()
        )

        raise HTTPException(
            status_code=502,
            detail=str(exc)
        ) from exc