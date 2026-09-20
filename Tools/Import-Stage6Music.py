#!/usr/bin/env python3
"""Import Stage 6's fourteen Avex race cues without decoding their MS ADPCM.

Original XWB payloads become RIFF/WAVE data chunks, with exact sample rates,
fact sample counts and smpl loops. XSB descriptors establish cue-to-wave IDs;
neither string order nor album order is used to select a wave-bank payload.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile

TITLE_SOURCE = "https://dengekionline.com/elem/000/000/886/886611/"
FORMAT_SOURCES = ["https://github.com/microsoft/DirectXTK/blob/main/Audio/WaveBankReader.cpp",
                  "https://github.com/FNA-XNA/FAudio/blob/master/src/FACT_internal.c"]
SONGS = (
    ("avex_01_super_rider", "Super Rider", "Mark Astley"),
    ("avex_02_rock_beaten_wild", "Rock Beatin' Wild", "Fastway"),
    ("avex_03_once_upon_a_time", "Once Upon a Time", "Pamsy"),
    ("avex_04_set_me_free", "Set Me Free", "Cherry"),
    ("avex_05_king_of_eurobeat", "King of Eurobeat", "Jordan"),
    ("avex_06_the_love_bite", "The Lovebite", "Dusty"),
    ("avex_07_euro_night", "Euronight", "Eurogroove"),
    ("avex_08_queen_of_meam", "Queen of Mean", "The Snake"),
    ("avex_09_mad_desire", "Mad Desire", "Stephy Martini"),
    ("avex_10_burn_into_the_beat", "Burn into the Beat", "Nick Mansell"),
    ("avex_11_forever_sad", "Forever Sad", "Hely"),
    ("avex_12_hurricane_man", "Hurricane Man", "Gold-Rake"),
    ("avex_13_dont_turn_it_off", "Don't Turn It Off", "Go 2"),
    ("avex_14_you_are_my_wonder", "You Are My Wonder", "Queen 26"),
)
PRIMARY_TITLE_SOURCES = (
    "https://avex.jp/eurobeat/release/detail.php?id=1014181",
    "https://scp-music.bandcamp.com/album/initial-beat-2-music-from-the-initiald-world",
    "https://shop.mu-mo.net/mitem/AVCD-10172?jsiteid=mumo",
    "https://avex.jp/eurobeat/news/detail.php?id=1112218",
    "https://avex.jp/eurobeat/release/detail.php?id=1015873",
    "https://shop.mu-mo.net/mitem/AVCD-10183?jsiteid=mumo",
    "https://shop.mu-mo.net/mitem/AVCD-10192?jsiteid=mumo",
    "https://avex.jp/eurobeat/release/detail.php?id=1015873",
    "https://shop.mu-mo.net/mitem/AVCD-10172?jsiteid=mumo",
    "https://shop.mu-mo.net/mitem/AVCD-10183?jsiteid=mumo",
    "https://shop.mu-mo.net/mitem/AVCD-10194?jsiteid=mumo",
    "https://avex.jp/chozen/discography/",
    "https://scp-music.bandcamp.com/album/initial-beat-2-music-from-the-initiald-world",
    "https://avex.jp/eurobeat/release/detail.php?id=1015873",
)
COEFFICIENTS = ((256,0),(512,-256),(0,0),(192,64),(240,0),(460,-208),(392,-232))


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def atomic_write(path: Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".stage6-", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def parse_cues(data: bytes) -> list[dict]:
    require(len(data) >= 138 and data[:4] == b"SDBK"
            and struct.unpack_from("<HH", data, 4) == (45,43) and data[18] == 1,
            "Expected little-endian Windows XACT 3.4 sound bank")
    simple, complex_count = struct.unpack_from("<HH", data, 19)
    require(simple == 7088 and complex_count == 0 and data[27] == 1,
            "Expected Stage 6's 7088 simple cue descriptors and one wave bank")
    offsets = struct.unpack_from("<10i", data, 34)
    cue_offset, names_offset, name_index = offsets[0], offsets[2], offsets[8]
    require(0 <= cue_offset <= len(data)-simple*5 and 0 <= name_index <= len(data)-simple*6,
            "XSB cue/name directory is out of bounds")
    require(data[74:138].split(b"\0",1)[0] == b"Inid6"
            and 0 <= offsets[6] <= len(data)-64
            and data[offsets[6]:offsets[6]+64].split(b"\0",1)[0] == b"Inid6",
            "XSB references an unexpected wave bank")
    cues = []
    for index in range(simple):
        name_at = struct.unpack_from("<I", data, name_index+index*6)[0]
        require(names_offset <= name_at < len(data), "XSB cue name is out of bounds")
        end = data.find(b"\0",name_at)
        require(end >= name_at, "Unterminated XSB cue name")
        name = data[name_at:end].decode("ascii")
        cue_flags, sound_at = struct.unpack_from("<BI", data, cue_offset+index*5)
        require(cue_flags == 4 and sound_at <= len(data)-12, "Unsupported XSB cue descriptor")
        flags = data[sound_at]
        sound_length = struct.unpack_from("<H",data,sound_at+7)[0]
        require(flags in (0,1) and sound_length == (36 if flags else 12)
                and sound_at+sound_length <= len(data), "Unsupported XSB sound record")
        if flags == 0:
            wave, bank = struct.unpack_from("<HB", data, sound_at+9)
            loop_count = 0
        else:
            event_at = struct.unpack_from("<I",data,sound_at+11)[0]
            require(data[sound_at+9] == 1 and event_at == sound_at+19
                    and data[event_at] == 1 and struct.unpack_from("<I",data,event_at+1)[0]&31 == 1
                    and data[event_at+7] == 255, "Expected one simple PLAYWAVE event")
            wave, bank, loop_count = struct.unpack_from("<HBB",data,event_at+9)
        require(bank == 0 and wave < simple, "XSB wave reference is invalid")
        cues.append(dict(cueIndex=index,code=name,waveIndex=wave,soundOffset=sound_at,
                         soundFlags=flags,loopCount=loop_count,
                         soundVolumeByte=data[sound_at+3],soundPitch=struct.unpack_from("<h",data,sound_at+4)[0]))
    require(len({cue["code"] for cue in cues}) == simple, "Duplicate XSB cue names")
    return cues


def parse_wave_bank(path: Path) -> list[dict]:
    size = path.stat().st_size
    with path.open("rb") as stream:
        header = stream.read(52)
        require(len(header) == 52 and header[:4] == b"WBND"
                and struct.unpack_from("<II",header,4) == (45,43), "Unexpected XWB version")
        flat = struct.unpack_from("<10I",header,12)
        segments = list(zip(flat[::2],flat[1::2]))
        for offset, length in segments:
            require(offset+length <= size, "XWB segment extends beyond file")
        require(segments[0][1] == 96, "Unexpected XWB bank metadata length")
        stream.seek(segments[0][0]);bank = stream.read(96)
        flags,count = struct.unpack_from("<II",bank)
        stride,name_stride,alignment = struct.unpack_from("<III",bank,72)
        require(flags == 0x80001 and count == 7088 and stride == 24 and name_stride == 64
                and alignment == 2048 and bank[8:72].split(b"\0",1)[0] == b"Inid6",
                "Unsupported Stage 6 wave-bank layout")
        require(segments[1][1] == count*stride, "XWB entry directory length mismatch")
        stream.seek(segments[1][0]);entries = stream.read(segments[1][1])
        waves = []
        for index in range(count):
            duration_flags, mini, play_offset, length, loop_start, loop_length = struct.unpack_from("<6I",entries,index*stride)
            channels=(mini>>2)&7;rate=(mini>>5)&0x3ffff;block=(((mini>>23)&255)+22)*channels
            samples_per_block=block*2//max(1,channels)-12;frames=duration_flags>>4
            require(duration_flags&15 == 0 and mini&3 == 2 and channels in (1,2)
                    and 8000 <= rate <= 48000 and samples_per_block > 0,
                    "Unsupported XWB sound codec/format")
            require(play_offset%alignment == 0 and play_offset+length <= segments[4][1]
                    and length > 0 and length%block == 0
                    and (length//block-1)*samples_per_block < frames <= length//block*samples_per_block,
                    "Invalid XWB payload/sample bounds")
            require(0 <= loop_start <= frames and loop_start+loop_length <= frames,
                    "Invalid XWB loop region")
            waves.append(dict(waveIndex=index,sampleRate=rate,channels=channels,frames=frames,
                              blockBytes=block,samplesPerBlock=samples_per_block,
                              sourceByteOffset=segments[4][0]+play_offset,encodedDataBytes=length,
                              loopStartFrames=loop_start,loopEndFrames=loop_start+loop_length,
                              sourceLoopLength=loop_length,sourceMiniFormat=mini))
    return waves


def chunk(tag: bytes, data: bytes) -> bytes:
    return tag + struct.pack("<I",len(data)) + data + (b"\0" if len(data)&1 else b"")


def wrap_wave(payload: bytes, wave: dict, loop: bool) -> tuple[bytes,dict]:
    rate=wave["sampleRate"];channels=wave["channels"];block=wave["blockBytes"];samples=wave["samplesPerBlock"]
    require(len(payload) == wave["encodedDataBytes"], "Truncated XWB payload")
    # DirectXTK's XACT MINIWAVEFORMAT expansion: standard seven coefficients,
    # nAvgBytesPerSec = nBlockAlign * nSamplesPerSec / samplesPerBlock.
    fmt=struct.pack("<HHIIHHHHH",2,channels,rate,block*rate//samples,block,4,32,samples,7)
    fmt+=b"".join(struct.pack("<hh",a,b) for a,b in COEFFICIENTS)
    parts=chunk(b"fmt ",fmt)+chunk(b"fact",struct.pack("<I",wave["frames"]))
    if loop:
        require(wave["loopStartFrames"] < wave["loopEndFrames"], "Looping XSB cue has no XWB loop region")
        # RIFF smpl stores an inclusive endpoint; the catalog/runtime use an
        # exclusive endpoint. XWB already provides start plus exact length.
        smpl=struct.pack("<9I",0,0,1_000_000_000//rate,60,0,0,0,1,0)
        smpl+=struct.pack("<6I",0,0,wave["loopStartFrames"],wave["loopEndFrames"]-1,0,0)
        parts+=chunk(b"smpl",smpl)
    data_offset=12+len(parts)+8
    parts+=chunk(b"data",payload)
    output=b"RIFF"+struct.pack("<I",4+len(parts))+b"WAVE"+parts
    require(output[data_offset:data_offset+len(payload)] == payload, "RIFF wrapper changed native samples")
    info=dict(sampleRate=rate,channels=channels,frames=wave["frames"],durationSeconds=wave["frames"]/rate,
              sha256=digest(output),bytes=len(output),format="WAVE",codec="Microsoft ADPCM",bitsPerSample=4,
              blockBytes=block,samplesPerBlock=samples,encodedDataOffset=data_offset,encodedDataBytes=len(payload),
              encodedDataSha256=digest(payload),looping=loop,
              loopStartFrames=wave["loopStartFrames"] if loop else 0,
              loopEndFrames=wave["loopEndFrames"] if loop else 0)
    return output,info


def prepare(xsb: Path,xwb: Path) -> tuple[list,dict]:
    sound_data=xsb.read_bytes();cues=parse_cues(sound_data);waves=parse_wave_bank(xwb)
    by_name={cue["code"]:cue for cue in cues}
    require({cue["code"] for cue in cues if cue["code"].startswith("avex_")}
            == {row[0] for row in SONGS}|{"avex_15_the_double_ace","avex_16_bird"},
            "Stage 6 Avex cue roster differs")
    for excluded in ("avex_15_the_double_ace","avex_16_bird"):
        require(by_name[excluded]["loopCount"] == 0, "Opening/ending unexpectedly marked as race loop")
    prepared=[]
    with xwb.open("rb") as stream:
        for code,title,artist in SONGS:
            cue=by_name[code];wave=waves[cue["waveIndex"]]
            require(cue["loopCount"] == 255 and wave["channels"] == 2 and wave["blockBytes"] == 44
                    and wave["samplesPerBlock"] == 32, "Unexpected Stage 6 race cue format")
            stream.seek(wave["sourceByteOffset"]);payload=stream.read(wave["encodedDataBytes"])
            output,metadata=wrap_wave(payload,wave,True)
            prepared.append((code,title,artist,cue,wave,output,metadata))
    with xwb.open("rb") as stream:
        bank_hash=hashlib.file_digest(stream,"sha256").hexdigest()
    provenance=dict(sourceSoundBank="Inid6.xsb",sourceSoundBankSha256=digest(sound_data),
                    sourceWaveBank="Inid6.xwb",sourceWaveBankSha256=bank_hash,
                    sourceCueCount=len(cues),sourceWaveCount=len(waves),formatSources=FORMAT_SOURCES,
                    excludedCueNames=["avex_15_the_double_ace","avex_16_bird"],
                    cueToWaveMapping="XSB cue-name index -> simple cue -> sound -> PLAYWAVE wave index",
                    transformation="RIFF/WAVE container only; original MS ADPCM payload bytes and loop points unchanged")
    return prepared,provenance


def run(project: Path,xsb: Path,xwb: Path,check: bool,extract_only: bool) -> dict:
    prepared,provenance=prepare(xsb,xwb)
    streams=project/"Native/data/original_audio/streams"
    for code,_,_,_,_,data,_ in prepared:
        path=streams/"stage6"/(code+".wav")
        require(not path.exists() or path.read_bytes() == data, "Refusing to overwrite differing Stage 6 asset: "+str(path))
        require(not check or path.exists(), "Stage 6 asset missing: "+str(path))
    if not check:
        for code,_,_,_,_,data,_ in prepared:
            atomic_write(streams/"stage6"/(code+".wav"),data)
    records=[];title_records=[]
    for ordinal,(code,title,artist,cue,wave,_,metadata) in enumerate(prepared):
        identity="stage6."+code
        records.append(dict(id=identity,sourceStage=6,code=code,filename=code+".wav",
                            path="data/original_audio/streams/stage6/"+code+".wav",
                            sourceCueIndex=cue["cueIndex"],sourceWaveIndex=cue["waveIndex"],
                            sourceSoundRecordOffset=cue["soundOffset"],sourceCueLoopCount=cue["loopCount"],
                            sourceArchive="Inid6.xwb",sourceArchiveByteOffset=wave["sourceByteOffset"],
                            sourceMiniFormat=wave["sourceMiniFormat"],sourceSoundVolumeByte=cue["soundVolumeByte"],
                            sourceSoundPitch=cue["soundPitch"],sourceWaveBankSha256=provenance["sourceWaveBankSha256"],
                            displayLabel=title,titleKnown=True,**metadata))
        title_records.append(dict(id=identity,title=title,artist=artist,sources=[PRIMARY_TITLE_SOURCES[ordinal],TITLE_SOURCE],
            fileMappingEvidence=f"Inid6.xsb cue {cue['cueIndex']} named {code}, sound record at {cue['soundOffset']}, "
            f"contains PLAYWAVE index {cue['waveIndex']} into Inid6.xwb. Exact encoded data starts at "
            f"{wave['sourceByteOffset']}; only a RIFF container was added. Display spelling corrects native cue "
            "abbreviations/typos using primary label/publisher credits and the cited Sega-series soundtrack roster."))
    if not extract_only:
        spec=importlib.util.spec_from_file_location("music_export",project/"Tools/Export-MusicCatalog.py")
        exporter=importlib.util.module_from_spec(spec);spec.loader.exec_module(exporter)
        header_path=project/"Native/src/music_catalog.h";header=header_path.read_text(encoding="utf-8-sig")
        entries=exporter.parse_entries(header)
        require(len(entries) in (58,72), "Expected 58 existing or 72 already-imported songs")
        old=json.loads((streams/"music_catalog.json").read_bytes())
        require(len(old["tracks"]) in (58,72), "Unexpected existing catalog size")
        for entry in old["tracks"][:58]:
            require(digest((streams/entry["relativePath"]).read_bytes()) == entry["sha256"], "Existing music payload changed")
        for (_,_,_,_,_,data,metadata) in prepared:
            require(exporter.inspect_msadpcm_wav(data,"Stage6") == metadata, "Exporter and XWB metadata disagree")
        manifest_path=streams/"extra_music_manifest.json";titles_path=streams/"music_title_metadata.json"
        manifest=json.loads(manifest_path.read_bytes());titles=json.loads(titles_path.read_bytes())
        preserved=[record for record in manifest["tracks"] if record["sourceStage"] != 6]
        preserved_titles=[record for record in titles["tracks"] if not record["id"].startswith("stage6.")]
        require(len(preserved) == 45 and len(preserved_titles) == 58, "Previous music metadata roster differs")
        manifest["tracks"]=preserved+records;manifest["trackCount"]=59
        manifest["stage6Import"]=provenance
        manifest["transformation"]="Stages1-5 native files unchanged; Stage6 MS ADPCM payloads unchanged inside RIFF/WAVE wrappers"
        titles["tracks"]=preserved_titles+title_records
        new_header=header
        if len(entries) == 58:
            declaration=exporter.DECLARATION.search(header);at=declaration.end(2)
            lines="".join("    {"+", ".join((json.dumps("stage6."+code),json.dumps(title),"6",
                         json.dumps("stage6/"+code+".wav"),json.dumps(artist)))+"},\n" for code,title,artist in SONGS)
            new_header=header[:at]+lines+header[at:]
            import re
            new_header=re.sub(r"(std::array\s*<\s*RaceMusicTrack\s*,\s*)58(\s*>)",r"\g<1>72\2",new_header)
        final_entries=exporter.parse_entries(new_header)
        require(final_entries[:58] == entries[:58]
                and [entry["id"] for entry in final_entries[58:]] == [record["id"] for record in records],
                "Append-only native music order changed")
        for entry,(_,title,artist) in zip(final_entries[58:],SONGS):
            require(entry["title"] == title and entry["artist"] == artist and entry["sourceStage"] == 6,
                    "Stage 6 native metadata differs")
        for path,data in ((header_path,new_header.encode("utf-8")),(manifest_path,json_bytes(manifest)),(titles_path,json_bytes(titles))):
            if check:require(path.read_text(encoding="utf-8-sig") == data.decode("utf-8"),"Imported metadata stale: "+str(path))
            else:atomic_write(path,data)
        result=exporter.export(project)
        require(result["tracks"][:58] == old["tracks"][:58], "Previously exported music records changed")
        if check:require(json.loads((streams/"music_catalog.json").read_bytes()) == result,"Exported catalog is stale")
        else:atomic_write(streams/"music_catalog.json",json_bytes(result))
        for path,value in manifest["verification"]["stage3Sha256"].items():
            require(digest((streams/path).read_bytes()) == value, "Original non-race Stage 3 stream changed")
    return dict(status="verified" if check else "extracted" if extract_only else "imported",trackCount=72,
                stage6Count=14,previous58CatalogRecordsUnchanged=not extract_only,
                encodedPayloadBytes=sum(record["encodedDataBytes"] for record in records),
                containerBytes=sum(record["bytes"] for record in records),
                provenance=provenance,tracks=[dict(index=58+i,**record) for i,record in enumerate(records)])


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root",type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument("--xsb",type=Path,required=True);parser.add_argument("--xwb",type=Path,required=True)
    parser.add_argument("--check",action="store_true",help="Verify assets/metadata without rewriting them")
    parser.add_argument("--extract-only",action="store_true",help="Prepare original payload wrappers before catalog integration")
    parser.add_argument("--report",type=Path)
    args=parser.parse_args();result=run(args.project_root.resolve(),args.xsb.resolve(),args.xwb.resolve(),args.check,args.extract_only)
    if args.report:atomic_write(args.report.resolve(),json_bytes(result))
    print(json.dumps({key:value for key,value in result.items() if key not in ("tracks","provenance")},indent=2))


if __name__ == "__main__":
    try:main()
    except (OSError,ValueError,KeyError,TypeError,struct.error) as error:
        raise SystemExit(f"Stage 6 import failed: {error}") from error
