#!/usr/bin/env bash
# Generate compile_commands.json for host clangd from the dev Docker image.
#
# PIO headers live in the image at /root/.platformio. clangd runs on the host,
# so this copies the packages the firmware build uses into .clangd-pio and
# rewrites the database to those paths. Extract via tar so files are owned by
# you, not root (docker cp would preserve root ownership).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

IMAGE_FILE="$ROOT/docker/.docker-image"
IMAGE="$(head -1 "$IMAGE_FILE" | tr -d '\n\r')"
PIO_HOME="$ROOT/.clangd-pio"
ENV_NAME="${HIL_CLANGD_ENV:-esp32-s3-devkitc-1}"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "Docker image not found: $IMAGE" >&2
    echo "Pull or build it with ./docker/dev.sh (no args) first." >&2
    exit 1
fi

img_id="$(docker image inspect -f '{{.Id}}' "$IMAGE")"
stamp="$PIO_HOME/.image-id"

echo "=== pio run -e $ENV_NAME -t compiledb ==="
./docker/dev.sh pio run -e "$ENV_NAME" -t compiledb

# Docker writes .pio as root. World-readable libdeps so host clangd can open ArduinoJson etc.
docker run --rm -v "$ROOT:/workspace" "$IMAGE" \
    chmod -R a+rX /workspace/.pio/libdeps

DB="$ROOT/compile_commands.json"
if [[ ! -f "$DB" ]]; then
    alt="$ROOT/.pio/build/$ENV_NAME/compile_commands.json"
    if [[ -f "$alt" ]]; then
        cat "$alt" >"$ROOT/.compile_commands.raw.json"
        DB="$ROOT/.compile_commands.raw.json"
    else
        echo "compile_commands.json was not generated" >&2
        exit 1
    fi
fi

RAW="$(mktemp)"
# Readable even when Docker created the file as root.
cat "$DB" >"$RAW"

mapfile -t PACKAGES < <(python3 - "$RAW" <<'PY'
import json, re, sys
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text())
pkgs, plats = set(), set()
for ent in data:
    text = ent.get("command") or " ".join(ent.get("arguments") or [])
    for key in ("directory", "file", "output"):
        if key in ent and isinstance(ent[key], str):
            text += " " + ent[key]
    pkgs.update(re.findall(r"/root/\.platformio/packages/([^/\s\"'\\]+)", text))
    plats.update(re.findall(r"/root/\.platformio/platforms/([^/\s\"'\\]+)", text))
for name in sorted(pkgs):
    print(f"packages/{name}")
for name in sorted(plats):
    print(f"platforms/{name}")
PY
)

need_seed=1
if [[ -f "$stamp" && "$(cat "$stamp")" == "$img_id" ]]; then
    need_seed=0
    for rel in "${PACKAGES[@]}"; do
        if [[ ! -d "$PIO_HOME/$rel" ]]; then
            need_seed=1
            break
        fi
    done
fi

if [[ "$need_seed" -eq 1 ]]; then
    echo "=== copy PIO packages from $IMAGE → .clangd-pio ==="
    mkdir -p "$PIO_HOME"
    for rel in "${PACKAGES[@]}"; do
        echo "  $rel"
        mkdir -p "$PIO_HOME/$(dirname "$rel")"
        docker run --rm "$IMAGE" tar -C /root/.platformio -cf - "$rel" \
            | tar -C "$PIO_HOME" -xf -
    done
    if [[ ! -x "$PIO_HOME/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-g++" ]]; then
        echo "  packages/toolchain-xtensa-esp32s3 (query-driver)"
        mkdir -p "$PIO_HOME/packages"
        docker run --rm "$IMAGE" tar -C /root/.platformio -cf - packages/toolchain-xtensa-esp32s3 \
            | tar -C "$PIO_HOME" -xf -
    fi
    echo "$img_id" >"$stamp"
fi

REWRITTEN="$(mktemp)"
python3 - "$RAW" "$REWRITTEN" "$ROOT" "$PIO_HOME" <<'PY'
import json, sys
from pathlib import Path

src, dest, root, pio_home = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
data = json.loads(Path(src).read_text())

def rewrite(s: str) -> str:
    return s.replace("/workspace", root).replace("/root/.platformio", pio_home)

for ent in data:
    for key in ("directory", "file", "output"):
        if key in ent and isinstance(ent[key], str):
            ent[key] = rewrite(ent[key])
    if "command" in ent and isinstance(ent["command"], str):
        ent["command"] = rewrite(ent["command"])
    if "arguments" in ent and isinstance(ent["arguments"], list):
        ent["arguments"] = [rewrite(a) if isinstance(a, str) else a for a in ent["arguments"]]

Path(dest).write_text(json.dumps(data, indent=2) + "\n")
print(f"rewrote {len(data)} entries")
PY

OUT="$ROOT/compile_commands.json"
if [[ ! -e "$OUT" ]] || [[ -w "$OUT" ]]; then
    mv "$REWRITTEN" "$OUT"
else
    # Docker created it as root; replace contents as root, then give it back.
    docker run --rm \
        -v "$ROOT:/workspace" \
        -v "$REWRITTEN:/cc.json:ro" \
        "$IMAGE" \
        cp /cc.json /workspace/compile_commands.json
    rm -f "$REWRITTEN"
fi
docker run --rm -v "$ROOT:/workspace" "$IMAGE" \
    chown "$(id -u):$(id -g)" /workspace/compile_commands.json
rm -f "$RAW" "$ROOT/.compile_commands.raw.json"

echo "=== clangd: compile_commands.json is ready. Reload the editor window. ==="
