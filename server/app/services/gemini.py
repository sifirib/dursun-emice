import base64

import httpx

from app.config import GEMINI_API_KEY, MODEL_NAME, HTTP_TIMEOUT
from app.prompt import SYSTEM_PROMPT, USER_PROMPT, STYLE_PROMPT


class GeminiService:

    def __init__(self):

        self.url = (
            f"https://generativelanguage.googleapis.com/v1beta/models/"
            f"{MODEL_NAME}:generateContent?key={GEMINI_API_KEY}"
        )

        self.headers = {
            "Content-Type": "application/json"
        }

    async def chat(
        self,
        audio_bytes: bytes,
        mime_type: str
    ) -> str:

        base64_audio = base64.b64encode(audio_bytes).decode("utf-8")

        payload = {
            "systemInstruction": {
                "parts": [
                    {
                        "text": SYSTEM_PROMPT
                    }
                ]
            },
            "contents": [
                {
                    "parts": [
                        {
                            "inlineData": {
                                "mimeType": mime_type,
                                "data": base64_audio
                            }
                        },
                        {
                            "text": USER_PROMPT
                        },
                        {
                            "text": STYLE_PROMPT
                        }
                    ]
                }
            ]
        }

        # NOT: onceki halde burada senkron "requests.post()" kullaniliyordu.
        # Bu fonksiyon "async def" olsa da requests bloklayici oldugu icin
        # Gemini cevap verene kadar (1-3+ saniye) FastAPI'nin tek event
        # loop'u tamamen kilitleniyor, o sirada gelen baska hicbir istege
        # (orn. Render'in kendi health check'i ya da ikinci bir /chat_raw)
        # cevap verilemiyordu. httpx.AsyncClient gercek async calisir.
        async with httpx.AsyncClient(timeout=HTTP_TIMEOUT) as async_client:

            response = await async_client.post(
                self.url,
                headers=self.headers,
                json=payload
            )

        if response.status_code != 200:
            raise RuntimeError(
                f"Gemini API hatası ({response.status_code}):\n{response.text}"
                )

        try:

            result = response.json()

            return (
                result["candidates"][0]
                ["content"]["parts"][0]
                ["text"]
                .strip()
            )

        except (KeyError, IndexError):

            raise RuntimeError(
                "Gemini beklenmeyen bir cevap döndürdü."
            )