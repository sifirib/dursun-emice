import base64

import requests

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

        response = requests.post(
            self.url,
            headers=self.headers,
            json=payload,
            timeout=HTTP_TIMEOUT
        )

        if response.status_code != 200:
            raise RuntimeError(
                f"Gemini API hatası ({response.status_code})"
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