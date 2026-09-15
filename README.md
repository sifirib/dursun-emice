# Dursun Emice

A wooden carpenter figure that sits outside my shop and talks to whoever walks by.

## What it does

An ultrasonic sensor detects when someone approaches. Dursun Emice plays a short pre-recorded greeting, listens until the person finishes speaking, and sends the recorded audio to a FastAPI server.

Gemini understands the audio and generates a short reply in character as Dursun Emice, a 68-year-old retired carpenter from the Black Sea region of Türkiye. The reply is converted to speech and sent back to the ESP32-S3 for playback.

As long as the same person remains nearby, the conversation can continue without replaying the greeting.

No screen, no buttons, nothing to press. Just a sensor, a mic, a speaker, and a Wi-Fi connection.

## How it's put together

**`embedded/`** — ESP32-S3 firmware built with PlatformIO. Handles visitor detection, voice activity detection, audio recording, server communication, conversation state, and MP3 playback.

**`server/`** — FastAPI backend. Takes WAV audio from the ESP32, sends it to Gemini, converts the generated response to speech, and returns MP3 audio. Currently deployed on Render.

## Hardware

- ESP32-S3-WROOM-1 N16R8
- HC-SR04 ultrasonic sensor
- INMP441 I2S microphone
- MAX98357A I2S amplifier
- Speaker

The HC-SR04 ECHO pin outputs 5V and needs a voltage divider before connecting to the ESP32.

## Setup

1. Copy `server/.env.example` to `server/.env` and set `GEMINI_API_KEY`.
2. Install server dependencies with `pip install -r server/requirements.txt`.
3. Run the server from the `server/` directory with `uvicorn app.main:app --reload --host 0.0.0.0`.
4. Copy `embedded/include/secrets.example.h` to `embedded/include/secrets.h` and set your Wi-Fi credentials and server URL.
5. Build and upload the firmware with PlatformIO.

## Status

**v0.1 Alpha**

The end-to-end conversation system is working on real hardware.

Current development is focused on better background-noise handling, neural voice activity detection, and more natural speech synthesis.