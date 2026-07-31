from io import BytesIO

import edge_tts

from app.config import TTS_VOICE


class TTSService:

    async def speak(self, text: str) -> bytes:

        communicate = edge_tts.Communicate(
            text=text,
            voice=TTS_VOICE
        )

        audio = BytesIO()

        async for chunk in communicate.stream():

            if chunk["type"] == "audio":
                audio.write(chunk["data"])

        return audio.getvalue()