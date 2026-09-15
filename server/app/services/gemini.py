import asyncio
import base64
import json

import httpx

from app.config import (
    GEMINI_API_KEY,
    MODEL_NAME,
    HTTP_TIMEOUT,
)

from app.prompt import (
    SYSTEM_PROMPT,
    USER_PROMPT,
    STYLE_PROMPT,
)


RETRYABLE_STATUS_CODES = {
    408,
    429,
    500,
    502,
    503,
    504,
}

MAX_ATTEMPTS = 2


class GeminiService:

    def __init__(self):

        self.url = (
            "https://generativelanguage.googleapis.com/"
            f"v1beta/models/{MODEL_NAME}:generateContent"
            f"?key={GEMINI_API_KEY}"
        )

        self.headers = {
            "Content-Type": "application/json"
        }

    async def chat(
        self,
        audio_bytes: bytes,
        mime_type: str
    ) -> str:

        if not audio_bytes:
            raise RuntimeError(
                "Gemini'ye bos ses verisi gonderildi."
            )

        base64_audio = (
            base64
            .b64encode(audio_bytes)
            .decode("utf-8")
        )

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
            ],

            "generationConfig": {
                "thinkingConfig": {
                    "thinkingLevel": "minimal"
                },
                "maxOutputTokens": 180
            }
        }

        async with httpx.AsyncClient(
            timeout=HTTP_TIMEOUT
        ) as async_client:

            for attempt in range(
                1,
                MAX_ATTEMPTS + 1
            ):

                try:

                    response = await async_client.post(
                        self.url,
                        headers=self.headers,
                        json=payload
                    )

                except httpx.HTTPError as exc:

                    print(
                        f"[GEMINI] HTTP istemci hatasi "
                        f"(deneme {attempt}/{MAX_ATTEMPTS}): "
                        f"{type(exc).__name__}: {exc}"
                    )

                    if attempt < MAX_ATTEMPTS:
                        await asyncio.sleep(attempt)
                        continue

                    raise RuntimeError(
                        f"Gemini ag hatasi: {exc}"
                    ) from exc

                if response.status_code == 200:
                    break

                print(
                    f"[GEMINI] API HTTP "
                    f"{response.status_code} "
                    f"(deneme {attempt}/{MAX_ATTEMPTS})"
                )

                print(
                    response.text[:3000]
                )

                if (
                    response.status_code
                    in RETRYABLE_STATUS_CODES
                    and attempt < MAX_ATTEMPTS
                ):
                    await asyncio.sleep(attempt)
                    continue

                raise RuntimeError(
                    "Gemini API hatasi "
                    f"({response.status_code}):\n"
                    f"{response.text}"
                )

            else:
                raise RuntimeError(
                    "Gemini istegi basarisiz."
                )

        try:

            result = response.json()

            candidates = result.get(
                "candidates",
                []
            )

            if not candidates:
                raise RuntimeError(
                    "Gemini candidate dondurmedi:\n"
                    f"{json.dumps(result, ensure_ascii=False)[:3000]}"
                )

            parts = (
                candidates[0]
                .get("content", {})
                .get("parts", [])
            )

            text_parts = []

            for part in parts:

                text_value = part.get("text")

                if text_value:
                    text_parts.append(text_value)

            text = " ".join(
                text_parts
            ).strip()

            if not text:
                finish_reason = (
                    candidates[0]
                    .get("finishReason")
                )

                raise RuntimeError(
                    "Gemini metin cevabi vermedi. "
                    f"finishReason={finish_reason}\n"
                    f"{json.dumps(result, ensure_ascii=False)[:3000]}"
                )

            usage = result.get(
                "usageMetadata",
                {}
            )

            print(
                "[GEMINI] "
                f"prompt={usage.get('promptTokenCount', '?')} "
                f"output={usage.get('candidatesTokenCount', '?')} "
                f"total={usage.get('totalTokenCount', '?')} "
                f"thoughts={usage.get('thoughtsTokenCount', 0)}"
            )

            return text

        except json.JSONDecodeError as exc:

            raise RuntimeError(
                "Gemini JSON cevabi okunamadi."
            ) from exc