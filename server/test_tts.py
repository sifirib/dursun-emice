import asyncio

from app.services.tts import TTSService


async def main():

    tts = TTSService()

    audio = await tts.speak(
        "Ula evlat, hoş geldin."
    )

    with open("test.mp3", "wb") as f:
        f.write(audio)


asyncio.run(main())