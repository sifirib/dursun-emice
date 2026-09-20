from pathlib import Path
import csv
import subprocess
import time

Import("env")


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))

PARTITIONS_FILE = (
    PROJECT_DIR
    / "partitions_sr.csv"
)

SRMODELS_FILE = (
    PROJECT_DIR
    / "models"
    / "srmodels.bin"
)


def get_model_partition():
    with PARTITIONS_FILE.open(
        "r",
        encoding="utf-8",
        newline=""
    ) as file:

        for line in file:
            line = line.strip()

            if not line:
                continue

            if line.startswith("#"):
                continue

            row = next(
                csv.reader(
                    [line],
                    skipinitialspace=True
                )
            )

            row = [
                value.strip()
                for value in row
            ]

            if row[0] != "model":
                continue

            if len(row) < 5:
                raise RuntimeError(
                    "model partition satiri gecersiz."
                )

            offset = int(row[3], 0)
            size = int(row[4], 0)

            return offset, size

    raise RuntimeError(
        "Partition tablosunda "
        "'model' partition bulunamadi."
    )


def flash_sr_models(source, target, env):
    if not PARTITIONS_FILE.exists():
        raise RuntimeError(
            f"Partition dosyasi bulunamadi: "
            f"{PARTITIONS_FILE}"
        )

    if not SRMODELS_FILE.exists():
        raise RuntimeError(
            f"srmodels.bin bulunamadi: "
            f"{SRMODELS_FILE}"
        )

    model_offset, partition_size = (
        get_model_partition()
    )

    model_size = SRMODELS_FILE.stat().st_size

    if model_size > partition_size:
        raise RuntimeError(
            "srmodels.bin model partition'a sigmiyor. "
            f"Model={model_size}, "
            f"Partition={partition_size}"
        )

    python_exe = env.subst("$PYTHONEXE")
    uploader = env.subst("$UPLOADER")
    upload_port = env.subst("$UPLOAD_PORT")
    upload_speed = env.subst("$UPLOAD_SPEED")

    chip = env.BoardConfig().get(
        "build.mcu",
        "esp32s3"
    )

    print()
    print(
        "[ESP-SR] Custom model flash ediliyor..."
    )

    print(
        f"[ESP-SR] Model: {SRMODELS_FILE}"
    )

    print(
        f"[ESP-SR] Offset: {hex(model_offset)}"
    )

    print(
        f"[ESP-SR] Size: {model_size} byte"
    )

    # Firmware upload sonrasi USB/COM'un
    # tekrar hazir olmasi icin kisa bekleme.
    time.sleep(1.5)

    command = [
        python_exe,
        uploader,

        "--chip",
        chip,

        "--port",
        upload_port,

        "--baud",
        upload_speed,

        "--before",
        "default-reset",

        "--after",
        "hard-reset",

        "write-flash",
        "-z",

        "--flash-mode",
        "keep",

        "--flash-freq",
        "keep",

        "--flash-size",
        "keep",

        hex(model_offset),
        str(SRMODELS_FILE),
    ]

    result = subprocess.run(
        command,
        check=False
    )

    if result.returncode != 0:
        raise RuntimeError(
            "ESP-SR model flash islemi basarisiz."
        )

    print(
        "[ESP-SR] Custom model flash tamamlandi."
    )


env.AddPostAction(
    "upload",
    flash_sr_models
)