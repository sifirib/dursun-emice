import math
import struct
import sys
import time
import wave
from array import array
from pathlib import Path

import serial
from serial import SerialException


BAUD_RATE = 921600
SERIAL_TIMEOUT = 0.5

MAGIC = b"AFECMP01"

SCRIPT_DIR = Path(__file__).resolve().parent
EMBEDDED_DIR = SCRIPT_DIR.parent

OUTPUT_DIR = (
    EMBEDDED_DIR /
    "afe_compare_output"
)


def open_serial(
    port: str,
) -> serial.Serial:

    while True:

        try:

            ser = serial.Serial(
                port=port,
                baudrate=BAUD_RATE,
                timeout=SERIAL_TIMEOUT,
                write_timeout=2,
                rtscts=False,
                dsrdtr=False,
            )

            print(
                f"Serial acildi: {port}"
            )

            return ser

        except SerialException as exc:

            print(
                f"Port henuz hazir degil: {exc}"
            )

            time.sleep(
                1
            )


def wait_for_esp(
    ser: serial.Serial,
) -> None:

    print(
        "ESP ile baglanti kuruluyor..."
    )

    while True:

        try:

            ser.write(
                b"PING\n"
            )

            ser.flush()


            deadline = (
                time.monotonic() +
                1.0
            )


            while (
                time.monotonic() <
                deadline
            ):

                raw = ser.readline()

                if not raw:
                    continue


                text = raw.decode(
                    "utf-8",
                    errors="ignore",
                ).strip()


                if text:
                    print(
                        f"[ESP] {text}"
                    )


                if (
                    text ==
                    "AFE_COMPARE_READY"
                ):
                    print()
                    print(
                        "ESP HAZIR."
                    )

                    return


            time.sleep(
                0.3
            )

        except SerialException as exc:

            raise RuntimeError(
                "ESP USB baglantisi koptu. "
                "Scripti yeniden calistir. "
                f"Detay: {exc}"
            ) from exc


def wait_for_message(
    ser: serial.Serial,
    expected: str,
) -> None:

    while True:

        try:

            raw = ser.readline()

        except SerialException as exc:

            raise RuntimeError(
                "Kayit sirasinda USB baglantisi koptu. "
                f"Detay: {exc}"
            ) from exc


        if not raw:
            continue


        text = raw.decode(
            "utf-8",
            errors="ignore",
        ).strip()


        if text:

            print(
                f"[ESP] {text}"
            )


        if expected in text:

            return


def read_exact(
    ser: serial.Serial,
    size: int,
) -> bytes:

    data = bytearray()


    while len(data) < size:

        try:

            chunk = ser.read(
                size - len(data)
            )

        except SerialException as exc:

            raise RuntimeError(
                "Binary ses aktarimi sirasinda "
                "USB baglantisi koptu."
            ) from exc


        if not chunk:
            continue


        data.extend(
            chunk
        )


    return bytes(
        data
    )


def find_magic(
    ser: serial.Serial,
) -> None:

    window = bytearray()


    while True:

        value = ser.read(
            1
        )


        if not value:
            continue


        window.extend(
            value
        )


        if len(window) > len(MAGIC):

            del window[0]


        if bytes(window) == MAGIC:

            return


def save_wav(
    path: Path,
    pcm: bytes,
    sample_rate: int,
) -> None:

    with wave.open(
        str(path),
        "wb",
    ) as wav_file:

        wav_file.setnchannels(
            1
        )

        wav_file.setsampwidth(
            2
        )

        wav_file.setframerate(
            sample_rate
        )

        wav_file.writeframes(
            pcm
        )


def calculate_stats(
    pcm: bytes,
) -> dict:

    samples = array(
        "h"
    )

    samples.frombytes(
        pcm
    )


    if sys.byteorder != "little":

        samples.byteswap()


    if not samples:

        return {
            "count": 0,
            "mean": 0.0,
            "rms": 0.0,
            "peak": 0,
        }


    total = 0.0
    squared_total = 0.0
    peak = 0


    for sample in samples:

        total += sample

        squared_total += (
            sample *
            sample
        )

        peak = max(
            peak,
            abs(sample),
        )


    count = len(
        samples
    )


    return {
        "count": count,

        "mean":
            total /
            count,

        "rms":
            math.sqrt(
                squared_total /
                count
            ),

        "peak":
            peak,
    }


def print_stats(
    name: str,
    stats: dict,
    sample_rate: int,
) -> None:

    duration = (
        stats["count"] /
        sample_rate
    )


    print()
    print(
        name
    )

    print(
        f"  Sure : {duration:.3f} s"
    )

    print(
        f"  Mean : {stats['mean']:.2f}"
    )

    print(
        f"  RMS  : {stats['rms']:.2f}"
    )

    print(
        f"  Peak : {stats['peak']}"
    )


def main() -> None:

    if len(sys.argv) != 2:

        print(
            "Kullanim:"
        )

        print(
            "python scripts/capture_afe_compare.py COM5"
        )

        raise SystemExit(
            1
        )


    port = sys.argv[1]


    OUTPUT_DIR.mkdir(
        parents=True,
        exist_ok=True,
    )


    ser = open_serial(
        port
    )


    try:

        # -------------------------------------------------
        # Handshake
        # -------------------------------------------------

        wait_for_esp(
            ser
        )


        print()
        print(
            "========================================"
        )

        print(
            "AYNI SES RAW + NSNET2 KAYDEDILECEK"
        )

        print()
        print(
            "ENTER'a bastiktan sonra:"
        )

        print(
            "1 saniye sessiz kal."
        )

        print(
            "Sonra normal sesle:"
        )

        print()

        print(
            '"Dursun Emice bugün nasılsın, '
            'beni düzgün duyabiliyor musun?"'
        )

        print(
            "========================================"
        )


        input(
            "\nHazirsan ENTER..."
        )


        # -------------------------------------------------
        # Capture
        # -------------------------------------------------

        ser.write(
            b"START\n"
        )

        ser.flush()


        wait_for_message(
            ser,
            "CAPTURE_DONE",
        )


        print()
        print(
            "Kayit tamamlandi."
        )

        print(
            "PCM verileri PC'ye aliniyor..."
        )


        # -------------------------------------------------
        # Binary dump
        # -------------------------------------------------

        ser.write(
            b"DUMP\n"
        )

        ser.flush()


        find_magic(
            ser
        )


        header = read_exact(
            ser,
            12,
        )


        (
            sample_rate,
            raw_samples,
            filtered_samples,
        ) = struct.unpack(
            "<III",
            header,
        )


        print()
        print(
            f"Sample rate: {sample_rate}"
        )

        print(
            f"RAW samples: {raw_samples}"
        )

        print(
            f"FILTERED samples: {filtered_samples}"
        )


        raw_pcm = read_exact(
            ser,
            raw_samples * 2,
        )


        filtered_pcm = read_exact(
            ser,
            filtered_samples * 2,
        )


    finally:

        ser.close()


    # -------------------------------------------------
    # WAV
    # -------------------------------------------------

    raw_path = (
        OUTPUT_DIR /
        "raw.wav"
    )


    filtered_path = (
        OUTPUT_DIR /
        "filtered.wav"
    )


    save_wav(
        raw_path,
        raw_pcm,
        sample_rate,
    )


    save_wav(
        filtered_path,
        filtered_pcm,
        sample_rate,
    )


    # -------------------------------------------------
    # Stats
    # -------------------------------------------------

    raw_stats = calculate_stats(
        raw_pcm
    )


    filtered_stats = calculate_stats(
        filtered_pcm
    )


    print_stats(
        "RAW",
        raw_stats,
        sample_rate,
    )


    print_stats(
        "FILTERED / NSNet2",
        filtered_stats,
        sample_rate,
    )


    print()
    print(
        "========================================"
    )

    print(
        "DOSYALAR HAZIR"
    )

    print()
    print(
        raw_path
    )

    print(
        filtered_path
    )

    print(
        "========================================"
    )


if __name__ == "__main__":

    main()