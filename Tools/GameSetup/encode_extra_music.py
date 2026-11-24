"""Prepare gapless race MP3s before packaging. Requires disc_music_pcm and FFmpeg with libmp3lame."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

from disc_extras import digest, song_paths


def inspect(decoder, path, *outputs):
    result = subprocess.check_output([decoder, path, *outputs], text=True)
    return tuple(map(int, result.split()))


def loop_tag(fields):
    # ID3v2.3 PRIV owner plus rate, channels, frame count, loop flag, start, end.
    payload = b"idas3.loop.v1\0" + struct.pack(">6I", *fields)
    frame = b"PRIV" + struct.pack(">I", len(payload)) + b"\0\0" + payload
    assert len(frame) < 128
    return b"ID3\x03\0\0\0\0\0" + bytes([len(frame)]) + frame


def encode(args, relative):
    source = args.streams / relative
    dest = (args.output / relative).with_suffix(".mp3")
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest.parent) as temp:
        work = Path(temp)
        pcm = work / "source.pcm"
        fields = inspect(args.decoder, source, pcm)
        rate, channels, frames, *_ = fields
        if pcm.stat().st_size != frames * channels * 2:
            raise ValueError(f"Incomplete source PCM for {relative}")
        mp3_rate = min((32000, 44100, 48000), key=lambda hz: abs(hz-rate))
        encoded_frame = lambda frame: (frame * mp3_rate + rate // 2) // rate
        expected = (mp3_rate, channels, encoded_frame(frames), fields[3],
                    encoded_frame(fields[4]), encoded_frame(fields[5]))
        encoded = work / "encoded.mp3"
        subprocess.run([args.ffmpeg, "-hide_banner", "-nostdin", "-loglevel", "error",
                        "-f", "s16le", "-ar", str(rate), "-ac", str(channels), "-i", pcm,
                        "-af", f"aresample={mp3_rate},apad=pad_len=8",
                        "-c:a", "libmp3lame", "-b:a", "160k", "-ar", str(mp3_rate), "-write_xing", "1",
                        "-id3v2_version", "0", encoded], check=True)
        tagged = work / "tagged.mp3"
        with tagged.open("wb") as output, encoded.open("rb") as audio:
            output.write(loop_tag(fields))
            shutil.copyfileobj(audio, output)
        if inspect(args.decoder, tagged) != expected:
            raise ValueError(f"Compressed music differs in duration or loop timing: {relative}")
        os.replace(tagged, dest)
    print(f"{relative}: {source.stat().st_size} -> {dest.stat().st_size} bytes", flush=True)
    return relative, {"source": digest(source), "mp3": digest(dest), "audio": fields}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("streams", "output", "decoder", "ffmpeg"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    names = sorted(song_paths(repository).values())
    if len({Path(n).with_suffix(".mp3") for n in names}) != len(names):
        raise ValueError("Duplicate race music output name")
    with ThreadPoolExecutor(max_workers=args.jobs) as workers:
        result = dict(workers.map(lambda name: encode(args, name), names))
    dest = args.output / "encoding.json"
    with tempfile.NamedTemporaryFile("w", dir=args.output, delete=False) as f:
        json.dump(result, f, sort_keys=True)
    os.replace(f.name, dest)


if __name__ == "__main__":
    main()
