import asyncio
from io import BytesIO

import edge_tts

from app.config import TTS_VOICE


MAX_ATTEMPTS = 2
TTS_TIMEOUT_SECONDS = 10


class TTSService:

    async def _speak_once(
        self,
        text: str
    ) -> bytes:

        communicate = edge_tts.Communicate(
            text=text,
            voice=TTS_VOICE
        )

        audio = BytesIO()

        async for chunk in communicate.stream():

            if chunk["type"] == "audio":
                audio.write(
                    chunk["data"]
                )

        result = audio.getvalue()

        if not result:
            raise RuntimeError(
                "Edge TTS ses verisi dondurmedi."
            )

        return result

    async def speak(
        self,
        text: str
    ) -> bytes:

        last_error = None

        for attempt in range(
            1,
            MAX_ATTEMPTS + 1
        ):

            try:

                print(
                    f"[TTS] Basliyor "
                    f"(deneme {attempt}/{MAX_ATTEMPTS})"
                )

                result = await asyncio.wait_for(
                    self._speak_once(text),
                    timeout=TTS_TIMEOUT_SECONDS
                )

                print(
                    f"[TTS] Basarili: "
                    f"{len(result)} byte"
                )

                return result

            except Exception as exc:

                last_error = exc

                print(
                    f"[TTS] HATA "
                    f"(deneme {attempt}/{MAX_ATTEMPTS}): "
                    f"{type(exc).__name__}: {exc}"
                )

                if attempt < MAX_ATTEMPTS:
                    await asyncio.sleep(1)

        raise RuntimeError(
            f"Edge TTS basarisiz: {last_error}"
        )