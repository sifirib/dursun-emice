# Dursun Emice

A wooden carpenter figure that sits outside my shop and talks to whoever walks by.

## What it does

There's an ultrasonic sensor hidden in the figure. When someone gets within about 50cm of it, it plays a short pre-recorded greeting, then listens for a few seconds, records whatever they say back, and sends it off to a small server. The server asks Gemini to come up with a reply in character (a 68-year-old retired carpenter from the Black Sea coast who's been sitting outside this shop for years), turns that reply into speech, and sends it back to be played through a speaker hidden in the figure.

No screen, no buttons, nothing to press. Just a sensor, a mic, a speaker, and a WiFi connection.

## How it's put together

Two separate pieces that don't know much about each other:

**`embedded/`** — ESP32 firmware, written with PlatformIO. Reads the sensor, records audio over I2S, POSTs it to the server, and streams the mp3 response straight into the speaker without buffering the whole thing in RAM first. Runs as a small state machine: idle → greeting → listening → waiting on the server → playing the response → back to idle.

**`server/`** — FastAPI app. Takes raw WAV audio in, sends it to Gemini along with a system prompt describing the character, converts the reply to speech with edge-tts, and streams the mp3 straight back. Deployed on Render.

## Hardware

- ESP32 (needs two I2S peripherals running at the same time — mic in, speaker out — which rules out the ESP8266)
- HC-SR04 ultrasonic sensor
- INMP441 I2S microphone
- MAX98357A I2S amp + a small speaker

Pin assignments are all in `embedded/include/config.h`. Watch out for the HC-SR04's echo pin — it outputs 5V and needs a voltage divider before it touches the ESP32.

## Setup

1. In `server/`, copy `.env.example` to `.env` and set `GEMINI_API_KEY`
2. `pip install -r server/requirements.txt`, then `uvicorn app.main:app`
3. Set your WiFi credentials and the server URL in `embedded/include/config.h`
4. Record a short greeting line, save it as `embedded/data/greeting.mp3`, and upload it to the ESP32 with `pio run --target uploadfs`
5. `pio run --target upload` to flash the firmware
