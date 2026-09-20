"""Extract exact35-car tuning records from the canonical owner-supplied image."""
import argparse, hashlib, json, pathlib, struct

CANONICAL_SHA = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def export(image_path, output):
    image = pathlib.Path(image_path).read_bytes()
    if hashlib.sha256(image).hexdigest() != CANONICAL_SHA:
        raise ValueError('Canonical program identity mismatch')
    def words(address, count):
        return struct.unpack_from('<' + 'I' * count, image, address - 0x0c020000)
    data = bytearray(b'IDASTN1\0' + struct.pack('<II', 1, 35))
    def put(*values): data.extend(struct.pack('<' + 'I' * len(values), *values))
    sources, strings = [], set()
    for car in range(35):
        row = words(0x0c30ecc0 + car * 32, 8)
        put(*row)
        counts = []
        for package in range(row[1]):
            pointer, count = words(row[0] + package * 8, 2)
            put(pointer, count); counts.append(count)
            for step in range(count):
                record = words(pointer + 20 * step, 5); put(*record); strings.add(record[3])
        for table, size, string_index in [(2, 6, 4), (4, 1, None), (6, 3, 2)]:
            for index in range(row[table + 1]):
                record = words(row[table] + size * 4 * index, size); put(*record)
                if string_index is not None: strings.add(record[string_index])
        sources.append({'car': car, 'row': [f'{v:08x}' for v in row], 'basic_steps': counts})
    put(len(strings))
    text_records = {}
    for address in sorted(strings):
        begin = address - 0x0c020000; end = image.index(0, begin)
        text = image[begin:end]; put(address, len(text)); data.extend(text)
        text_records[f'{address:08x}'] = text.decode('euc_jp')
    output = pathlib.Path(output); output.mkdir(parents=True, exist_ok=True)
    (output / 'tables.idastune').write_bytes(data)
    (output / 'manifest.json').write_text(json.dumps({'schema': 'idas3-original-tuning-v1', 'program_sha256': CANONICAL_SHA,
        'table_sha256': hashlib.sha256(data).hexdigest(), 'source_table': '0c30ecc0', 'cars': sources,
        'descriptions_euc_jp': text_records}, indent=2, ensure_ascii=True) + '\n', encoding='utf-8')
    print(f'Exported35 cars, {len(data)} bytes, {len(strings)} original descriptions')
if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('image');p.add_argument('output');a=p.parse_args();export(a.image,a.output)
