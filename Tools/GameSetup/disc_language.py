"""Convert the player's Japanese Rev C HOSTFS artwork to the native menu layout."""
import hashlib
import json
from pathlib import Path
import shutil
import tempfile


def checksum(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


SPRITES = ("original_rip.bin", "original_rip.tbl", "sprites.idasui", "textures/textures.idastex")


def reference(localization):
    """Packaging reads developer-side assets and ships only their names and checksums."""
    records = json.loads((localization / "manifest.json").read_text(encoding="utf-8"))["banks"]
    art = localization / "original_assets"
    files = {"manifest.json": checksum(localization / "manifest.json")}
    for record in records:
        mesh = Path(record["asset"])
        for path in (mesh, mesh.parent / "textures/textures.idastex"):
            files["original_assets/" + path.as_posix()] = checksum(art / path)
    for name in SPRITES:
        path = "saved_data/password_sprite/" + name
        files["original_assets/" + path] = checksum(art / path)
    return files


def convert(host, english, output, expected):
    from extract_original_menus import export_menu, export_model_menu
    from import_japanese_ui import import_ui

    meshes = sorted(name.removeprefix("original_assets/") for name in expected if name.endswith(".idasmesh"))
    if not meshes or output.exists():
        raise ValueError("A new output directory and the Japanese artwork reference are required.")
    model = host / "model"
    tables = {}
    wanted = {Path(name).stem + "_pol.tbl" for name in meshes}
    for table in model.rglob("*_pol.tbl"):
        if table.name in wanted:
            if table.name in tables:
                raise ValueError(f"Ambiguous disc model: {table.name}")
            tables[table.name] = table
    missing = wanted - tables.keys()
    if missing:
        raise ValueError("Missing original model tables: " + ", ".join(sorted(missing)))
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as temp:
        stage = Path(temp)
        mapped = stage / "mapped/original_assets"
        # Several screens draw the same model at different paths. Decode it once.
        for name in meshes:
            path = Path(name)
            bank = path.stem
            decoded = stage / "banks" / bank
            if not decoded.exists():
                table = tables[bank + "_pol.tbl"]
                export_model_menu(table.parent, decoded, "twiddled", table.name, bank + "_tex.tbl")
            dest = stage / "japanese" / path.parent
            (dest / "textures").mkdir(parents=True, exist_ok=True)
            for child in (path.name, "textures/textures.idastex"):
                shutil.copyfile(decoded / child, dest / child)
        import_ui(english, stage / "japanese", mapped, banks=meshes)
        sprite = stage / "sprite"
        export_menu(host / "sprite/password", sprite, "twiddled")
        for name in SPRITES:
            dest = mapped / "saved_data/password_sprite" / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(sprite / name, dest)
        for name, digest in expected.items():
            file = mapped.parent / name
            if not file.is_file() or checksum(file) != digest:
                raise ValueError(f"Japanese Rev C artwork did not match the reference: {name}")
        mapped.parent.rename(output)
