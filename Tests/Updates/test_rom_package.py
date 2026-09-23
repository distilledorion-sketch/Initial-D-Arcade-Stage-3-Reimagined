"""Release inventory never reads original ROMs or retains ROM instructions."""
from pathlib import Path
import importlib.util
import io
import zipfile

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('pack', root/'Tools/Build-UpdatePatch.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)
required = {name: b'game fixture' for name in pack.REQUIRED}


def inventory(extra):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w') as archive:
        for name, contents in {**required, **extra}.items():
            archive.writestr(name, contents)
    data.seek(0)

    class NoRomReads(zipfile.ZipFile):
        def open(self, name, *args, **kwargs):
            path = name.filename if isinstance(name, zipfile.ZipInfo) else name
            assert path in required, 'Inventory tried to read a ROM or its README: '+path
            return super().open(name, *args, **kwargs)

    with NoRomReads(data) as archive:
        return pack.inventory(archive)


assert set(inventory({'rom/README.txt': b'setup instructions'})) == set(required)
rejected = [
    'rom/gds-0033.chd', 'ROM/gds-0033.chd', 'rom/gds-0033.cue',
    *(f'rom/gds-0033-track{i}.bin' for i in range(1, 4)),
    'rom/other.zip', 'rom/readme.txt', 'rom/subfolder/README.txt',
    'InitialDUnity_Data/rom/gds-0033.chd',
]
for name in rejected:
    try:
        inventory({name: b'private fixture, not a valid dump'})
    except AssertionError as error:
        assert str(error).startswith('Original ROM files must not enter release packages:'), (name, error)
    else:
        raise AssertionError('Inventory accepted a ROM path: '+name)
print(f'PASS: README excluded; {len(rejected)} ROM paths rejected without reading or hashing their contents')
