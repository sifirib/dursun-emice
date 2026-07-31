import base64

import httpx

from app.config import GEMINI_API_KEY, MODEL_NAME
from app.models import ChatResponse
from app.prompt import SYSTEM_PROMPT, STYLE_PROMPT, USER_PROMPT


class GeminiService:

    def __init__(self):
        self.url = (
            f"https://generativelanguage.googleapis.com/v1beta/models/"
            f"{MODEL_NAME}:generateContent?key={GEMINI_API_KEY}"
        )

    async def chat(self, audio_bytes: bytes) -> ChatResponse:

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
                            "text": USER_PROMPT
                        },
                        {
                            "text": STYLE_PROMPT
                        }
                    ]
                }
            ]
        }

        try:

            async with httpx.AsyncClient(timeout=60) as client:

                response = await client.post(
                    self.url,
                    headers={
                        "Content-Type": "application/json"
                    },
                    json=payload
                )

            response.raise_for_status()

            data = response.json()

            text = (
                data["candidates"][0]
                ["content"]["parts"][0]
                ["text"]
                .strip()
            )

            return ChatResponse(
                success=True,
                text=text
            )

        except httpx.HTTPError:

            return ChatResponse(
                success=False,
                text="He ya... bugün kafam biraz dalgın galiba evlat."
            )

        except (KeyError, IndexError):

            return ChatResponse(
                success=False,
                text="Ula evlat, bir an aklım dağıldı. Bir daha söyler misin?"
            )