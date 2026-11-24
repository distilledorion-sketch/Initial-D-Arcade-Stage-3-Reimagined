"""Regenerate course tree placements and draw order from the original program."""
import json
import math
from pathlib import Path
import struct
import subprocess

def matrix(angle):
    a = angle * (math.tau / 65536)
    c, s = math.cos(a), math.sin(a)
    return struct.pack("<16f", c,0,-s if angle else 0,0, 0,1,0,0, s,0,c,0, 0,0,0,1)

def layout_for_packaging(native, program, object_export):
    """Recover only field locations and grid parameters. No meshes or placement records enter the installer."""
    layouts = {}
    for ci, pair in object_export.OWNERS.items():
        directory = native / "data/original_models/courses" / object_export.COURSES[ci]
        for night, descriptor in enumerate(pair):
            constructor = descriptor[0]
            start = constructor - object_export.BASE
            raw = (directory / ("scene_" + ("night" if night else "day") + "_forward.idasobjects")).read_bytes()
            owners = []
            pos = 28
            for _ in range(struct.unpack_from("<I", raw, 16)[0]):
                mode, = struct.unpack_from("<I", raw, pos)
                basis = raw[pos+4:pos+68]
                floats = struct.unpack("<16f", basis)
                angle = round(math.atan2(floats[8], floats[0]) * 65536 / math.tau)
                if matrix(angle) != basis:
                    raise ValueError("Unrecognized original tree grid rotation.")
                values = struct.unpack_from("<4I2I2I", raw, pos+68)
                # Constructor float constants can be in a shared SH4 literal pool.
                addresses = []
                for index in (0,1,2,3,6,7):
                    value = values[index]
                    if value == 0:
                        addresses.append(None)
                        continue
                    encoded = struct.pack("<I", value)
                    address = program.find(encoded, start, start+0x1800)
                    if address < 0:
                        address = program.find(encoded, 0x190000-0x20000, 0x1b0000-0x20000)
                    if address < 0:
                        raise ValueError("Missing original course constructor constant.")
                    addresses.append(address)
                owners.append(dict(mode=mode,angle=angle,grid=values[4:6],words=addresses))
                count, = struct.unpack_from("<I", raw, pos+100)
                pos += 104+44*count
            key = str(constructor)
            if key in layouts and owners != layouts[key]:
                raise ValueError("Original tree constructor differs across uses.")
            layouts[key] = owners
    return layouts

def generate(game, project, helpers, log):
    from export_original_course_objects import BASE, COURSES, OWNERS, source_paths, read_owners, write_sidecar
    image = game / "idas3_main_0C020000.bin"
    code = image.read_bytes()
    host = game / "driveA/HOSTFS"
    layouts = json.loads((Path(__file__).parent / "object-layout.json").read_text())
    scratch = project / "verification/objects"
    scratch.mkdir(parents=True,exist_ok=True)
    for ci,cid in enumerate(COURSES):
        folder = project / "data/original_models/courses" / cid
        for variant in json.loads((folder / "banks_manifest.json").read_text())["variants"]:
            night, wet = variant["night"], variant.get("weather", 0)
            descriptor = OWNERS[ci][int(bool(night or wet))] if ci in OWNERS else None
            setup = []
            if descriptor:
                for (_,path), layout in zip(source_paths(code, descriptor), layouts[str(descriptor[0])], strict=True):
                    words = [struct.unpack_from("<I",code,a)[0] if a is not None else 0 for a in layout["words"]]
                    basis = struct.unpack("<16I", matrix(layout["angle"]))
                    setup.append(dict(path=path,sector_mode=layout["mode"],
                                      settings_words=[*basis,*words[:4],*layout["grid"],*words[4:]]))
            for reverse in (0,1):
                name = ("night" if night else "day")+("_reverse" if reverse else "_forward")+("_wet" if wet else "")
                original = json.loads((folder / (name+"_capture.json")).read_text())
                capture = scratch / (cid+"_"+name+".json")
                cmd = [helpers / "capture_course_object_selections.exe",image,ci*2+night,reverse,
                       original["path_count"],variant["world_chunk_count"],capture,wet]
                lamp_chunks = {1:4,4:72,5:189}
                if ci in lamp_chunks and (night or wet or ci == 4):
                    cmd.extend([host / "path" / (cid + "_path_lamp.bin"), lamp_chunks[ci]])
                subprocess.run(list(map(str,cmd)),stdout=log,stderr=subprocess.STDOUT,check=True)
                result = json.loads(capture.read_text())
                if result["boundaries"] != original["boundaries"]:
                    raise ValueError("Course tree sector mismatch.")
                owners = read_owners(code,host,descriptor,{"owners":setup},result["boundaries"]) if descriptor else []
                write_sidecar(folder/("scene_"+name+".idasobjects"),result,owners,variant["world_chunk_count"])
        print("Generated course foliage:",cid,flush=True)

