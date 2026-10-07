"""Read YABX object databases without executing the game's DLLs.

Layout traced in IDZero's yabukita_vc110_x64.dll: processHeader 0x18000a160,
buildObjects 0x180005660, processField 0x180009dd0, readRawString 0x1800175c0.
Fields remain raw bytes; the scene converter interprets their declared names.
"""
from pathlib import Path
import struct


class Database:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        self.at = 0
        assert self.take(4) == b'YABX', self.path
        version, flags, size, crc = self.unpack('HHII')
        assert version == 1 and flags == 0 and size == len(self.data)-16
        self.metadata = {}
        while (tag := self.unpack('B')[0]):
            self.metadata[tag] = self.string(False)
        self.name = self.string()
        self.libraries = {}
        while (name := self.string()):
            self.libraries[name] = self.unpack('I')[0]
        self.classes = [None]
        while (name := self.string()):
            required, parent = self.unpack('BH')
            assert parent < len(self.classes)
            fields = []
            while (field := self.string()):
                field_flags, field_size = self.unpack('BH')
                fields.append((field, field_size))
            self.classes.append(dict(name=name, parent=parent, fields=fields))
        self.exports = {}
        while (name := self.string()):
            self.exports[name] = self.unpack('H')[0]-10001
        self.imports = []
        while (name := self.string()):
            self.imports.append(name)
        count = self.unpack('H')[0]
        self.objects = []
        for index in range(count):
            kind, size = self.unpack('HI')
            end = self.at + size
            cls = self.classes[kind]
            obj = dict(_class=cls['name'], _index=index)
            while cls:
                for name, fixed_size in cls['fields']:
                    length = fixed_size or self.unpack('I')[0]
                    if name == 'file' and fixed_size == 24:
                        length = self.unpack('I')[0]
                    assert length <= end-self.at, (index, obj['_class'], name, self.at, length, end)
                    obj[name] = self.take(length)
                cls = self.classes[cls['parent']]
            assert self.at == end, (index, obj['_class'], self.at, end)
            self.objects.append(obj)
        assert not any(self.data[self.at:]), (self.at, len(self.data))

    def take(self, size):
        assert 0 <= size <= len(self.data)-self.at, (self.at, size)
        result = self.data[self.at:self.at+size]
        self.at += size
        return result

    def unpack(self, fmt):
        return struct.unpack('<'+fmt, self.take(struct.calcsize('<'+fmt)))

    def string(self, short=True):
        size = self.unpack('B' if short else 'H')[0]
        return self.take(size).rstrip(b'\0').decode('utf-8')

    def refs(self, field):
        count = struct.unpack_from('<I', field)[0]
        assert len(field) == 4+count*2
        return [self.ref(i) for i in struct.unpack_from('<'+'H'*count, field, 4)]

    def ref(self, value):
        if isinstance(value, bytes):
            value, = struct.unpack('<H', value)
        if value == 0:
            return None
        assert 10001 <= value < 10001+len(self.objects), value
        return self.objects[value-10001]


def string(field):
    count, = struct.unpack_from('<H', field)
    assert len(field) == count+2
    return field[2:].rstrip(b'\0').decode('utf-8')


if __name__ == '__main__':
    import sys
    from collections import Counter
    db = Database(sys.argv[1])
    print(db.name, len(db.objects), Counter(o['_class'] for o in db.objects))
    for obj in db.objects[:12]:
        print(obj['_index'], obj['_class'], {k: (len(v),v[:48].hex(' ')) for k,v in obj.items() if not k.startswith('_')})
