import base64

import httpx

from app.config import GEMINI_API_KEY, MODEL_NAME
from app.prompt import SYSTEM_PROMPT


class GeminiService:

    def __init__(self):
        self.url = (
            f"https://generativelanguage.googleapis.com/v1beta/models/"
            f"{MODEL_NAME}:generateContent?key={GEMINI_API_KEY}"
        )

    async def chat(self, audio_bytes: bytes) -> str:

        base64_audio = base64.b64encode(audio_bytes).decode()

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
                                "mimeType": "audio/wav",
                                "data": base64_audio
                            }
                        },
                        {
                            "text": (
                                "Kullanıcının gönderdiği ses kaydını dinle "
                                "ve yalnızca Dursun Emice olarak cevap ver."
                            )
                        }
                    ]
                }
            ]
        }

        async with httpx.AsyncClient(timeout=60) as client:

            response = await client.post(
                self.url,
                headers={
                    "Content-Type": "application/json"
                },
                json=payload
            )

        print(response.status_code)

        data = response.json()
        print(data)

        return "Henüz tamamlanmadı."