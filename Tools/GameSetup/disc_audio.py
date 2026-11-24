"""Materialize sound driver records and music scores from a verified disc extraction."""
import concurrent.futures
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

def generate(game, project, converters, dependency_path=None, only_missing=False):
    host = game / "driveA/HOSTFS"
    program = (game / "idas3_main_0C020000.bin").read_bytes()
    banks = host / "sound/pack"
    audio = project / "data/original_audio"
    # Each 80-byte descriptor starts with the filename, followed by its command and volume.
    cues = {}
    for index in range(39):
        start = 0x31df04 - 0x20000 + 80 * index
        filename = program[start:start + 64].split(b"\0", 1)[0].decode("ascii")
        name = Path(filename).stem
        if filename != name + ".bin" or not name.replace("_", "").isalnum():
            raise ValueError("Unexpected disc music filename.")
        cues.setdefault(name, index)
    with tempfile.TemporaryDirectory(prefix=".sound-", dir=project.parent) as temporary:
        scratch = Path(temporary)
        env = dict(os.environ, IDAS3_AUDIO_PROJECT=str(project), IDAS3_AUDIO_WORK=str(scratch),
                   IDAS3_AUDIO_DRIVER=str(host / "binary/AICADRV.bin"))
        if dependency_path:
            env["PYTHONPATH"] = str(dependency_path)
        def command(filename, *args, scope, log):
            cmd = [sys.executable, str(converters / (filename + ".py")), *map(str,args)]
            result = subprocess.run(cmd, env=scope, stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                log.flush()
                tail = Path(log.name).read_bytes()[-2000:].decode(errors="replace")
                raise ValueError(f"Sound conversion failed during {filename}:\n{tail}")
        with (project.parent / (project.name + "-sounds.log")).open("wb") as log:
            command("import_original_oneshot", scope=env, log=log)
        def score(item):
            name, index = item
            directory = audio / ("selection" if index < 3 else "music")
            directory.mkdir(parents=True, exist_ok=True)
            dest = directory / (name.lower() + ".idms")
            if only_missing and dest.is_file():
                return
            # Keep captures private to this invocation and release them after each cue.
            with tempfile.TemporaryDirectory(prefix=name + "-", dir=scratch) as work:
                work = Path(work)
                original = banks / (name + ".bin.nz")
                shutil.copyfile(original, work / (name + "_sound.bin"))
                shutil.copyfile(original, directory / (name + ".dtpk"))
                with (project.parent / (project.name + "-" + name + ".log")).open("wb") as log:
                    command("reference_original_music_sequence", name, "--cue-id", index,
                            scope=dict(env, IDAS3_AUDIO_WORK=str(work)), log=log)
                    command("fix_deferred_pitch", dest, scope=env, log=log)
                dest.with_suffix(".idms.lagged").unlink(missing_ok=True)
                print("Generated music:", name, flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
            list(workers.map(score, cues.items()))

