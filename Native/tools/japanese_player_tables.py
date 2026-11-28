"""Prepare the development-only native tables for a player built from Rev C."""
import argparse
from pathlib import Path
import re
import struct

from arcade_program import BASE, JAPANESE, identify


def generate(program, destination):
    image = program.read_bytes()
    if identify(image) != JAPANESE:
        raise ValueError("The Japanese native player requires the verified GDS-0032C program")
    destination.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parents[1] / "src"

    def words(address, count):
        offset = address - BASE
        if offset < 0 or offset + 4 * count > len(image):
            raise ValueError(f"Regional table outside the program at {address:08X}")
        return struct.unpack_from(f"<{count}I", image, offset)

    def word(address):
        return words(address, 1)[0]

    def quoted(address):
        offset = address - BASE
        if not 0 <= offset < len(image):
            raise ValueError(f"Text outside the program at {address:08X}")
        value = image[offset:image.index(0, offset, offset + 1024)]
        return '"' + ''.join(f'\\x{b:02x}' for b in value) + '"'

    def values(items):
        return '{' + ','.join(f'0x{n:08x}u' for n in items) + '}'

    def write(name, lines):
        text = '\n'.join(lines) + '\n'
        path = destination / name
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            path.write_text(text, encoding="utf-8", newline='\n')

    # 104400 builds a character-indexed descriptor array. Slot zero has
    # its own literal, followed by the destination pointer and slots 1..31.
    records, remap, ranges = [], [], []
    speech_ranges, speech_anchors = [], []
    for character in range(32):
        descriptor = word(0x0C104534 if character == 0 else 0x0C104538 + character * 4)
        table, count = words(descriptor, 2)
        if not 0 < count <= 32:
            raise ValueError("Unexpected Rev C conversation table size")
        entries = [words(table + 44 * row, 11) for row in range(count)]
        # 0C0F4800 indexes a separate array of (x, y, side) per picture.
        for entry in entries:
            anchors, speakers = entry[9:11]
            if not 1 <= speakers <= 10:
                raise ValueError("Invalid Japanese dialogue speaker count")
            speech_ranges.append((len(speech_anchors), speakers))
            for speaker in range(speakers):
                x, y, side = struct.unpack_from('<ffI', image, anchors - BASE + 12 * speaker)
                if not (x.is_integer() and y.is_integer() and side <= 1):
                    raise ValueError("Invalid Japanese dialogue position")
                speech_anchors.append((int(x), int(y)))
        if character == 31:
            if count != 19:
                raise ValueError("Invalid Bunta scene table")
            write("original_bunta_dialog_tables.inc", [
                'inline constexpr std::array<std::array<std::uint32_t,11>,19> buntaDialogElements{{',
                *(values(row) + ',' for row in entries), '}};'])
        else:
            ranges.append((len(records), count))
            records.extend(entries)
            mapping = words(word(0x0C326454 + character * 4), count)
            remap.extend(x if x < 128 else -1 for x in mapping)

    write("rival_speech.inc", [
        'constexpr SpeechRange speechRanges[]{' + ','.join('{%d,%d}' % row for row in speech_ranges) + '};',
        'constexpr SpeechAnchor speechAnchors[]{' + ','.join('{%d,%d}' % row for row in speech_anchors) + '};'])

    # The host uses the same glyph index conventions and shared interpreter
    # constants. Replace the declarations whose contents belong to the disc.
    dialogue = (source / "original_rival_dialog_tables.inc").read_text(encoding="utf-8")
    replacements = {
        "scrollRecords": "inline constexpr std::array<OriginalScrollRecord,%d> scrollRecords{{%s}};" %
                         (len(records), ','.join('{' + values(row) + '}' for row in records)),
        "scrollTables": "inline constexpr std::array<std::pair<std::uint16_t,std::uint16_t>,31> scrollTables{{%s}};" %
                        ','.join('{%d,%d}' % row for row in ranges),
        "portraitPartRemap": "inline constexpr std::array<std::int8_t,%d> portraitPartRemap{%s};" %
                             (len(remap), ','.join(map(str, remap))),
        "carNames": "inline constexpr std::array<std::string_view,35> carNames{%s};" %
                    ','.join(map(quoted, words(0x0C3188A4, 35))),
        "speakerColours": "inline constexpr std::array<std::uint32_t,10> speakerColours%s;" %
                          values(words(0x0C326984, 10)),
        # Japanese speech slots have no localized speaker-name prefix.
        "speakerNameStrings": "", "speakerNameLists": "", "nameSeparator": "",
    }
    for key, value in replacements.items():
        dialogue, count = re.subn(r'^inline constexpr .*?\b' + key + r'[({].*?;$', lambda _: value,
                                  dialogue, flags=re.MULTILINE)
        if count != 1:
            raise ValueError(f"Missing regional declaration {key}")
    write("original_rival_dialog_tables.inc", dialogue.splitlines())

    lecture = []
    for kind, (table, count) in enumerate(((0x0C344464, 8), (0x0C3446B8, 3), (0x0C344A4C, 6))):
        lecture.append(f"constexpr std::array<OriginalTimeAttackAdviceText,{count}> advice{kind}{{{{")
        for index in range(count):
            address = table + index * 16
            pointers = words(address, 4)
            texts = ','.join(map(quoted, pointers))
            lecture.append(f'{{0x{address:08x}u,{values(pointers)},{{{texts}}}}},')
        lecture.append('}};')
    lecture.append('constexpr std::array<std::array<std::uint32_t,7>,9> thresholds{{')
    for course in range(9):
        lecture.append(values(word(0x0C29823C + 36 * course + i) for i in (0, 4, 8, 12, 20, 24, 28)) + ',')
    lecture.append('}};')
    for kind, (table, count) in enumerate(((0x0C343560, 8), (0x0C3435A0, 3), (0x0C3435B8, 3))):
        lecture.append(f'constexpr std::array<unsigned,{count}> predicates{kind}' +
                       values(word(table + 8*i + 4) for i in range(count)) + ';')
    lecture.append('constexpr std::array<unsigned,9> sectionCounts' + values(words(0x0C344B48, 9)) + ';')
    lecture.append('constexpr std::array<unsigned,35> sixSpeed' +
                   values(word(0x0C2732FC + 8*i) for i in range(35)) + ';')
    for name, table, count in (('circuit', 0x0C344ADC, 3), ('mountain', 0x0C344B38, 4)):
        pointers = words(table, count)
        lecture.append(f'constexpr std::array<unsigned,{count}> {name}Addresses' + values(pointers) + ';')
        lecture.append(f'constexpr std::array<std::string_view,{count}> {name}Text{{' +
                       ','.join(map(quoted, pointers)) + '};')
    write("original_time_attack_analysis_tables.inc", lecture)

    courses = ['constexpr std::array<CourseRow,9> courses{{']
    for course in range(9):
        banner, background, count = words(0x0C2A9D38 + 12 * course, 3)
        table = word(0x0C347728 + 4 * course)
        if not 1 <= count <= 6:
            raise ValueError("Invalid Legend opponent count")
        rows = ','.join(values(words(table + i*32, 8)) for i in range(count))
        courses.append(f'{{{banner},{background},{count},{{{{{rows}}}}}}},')
    write("legend_courses.inc", [*courses, '}};'])
    write("appearance_reset.inc", [
        'constexpr std::array<unsigned,11> resetMethods' +
        values((*words(0x0C028950, 10), 0x0C0287A0)) + ';'])
    # Keep the original Rev C words so setup can restore the tables from
    # a Japanese disc. Draw commands use common numbering (extra slot 1).
    def ranking_labels(address, count):
        slots = words(address, count)
        if any(slot < 1 or slot >= 38 for slot in slots):
            raise ValueError("Unexpected regional ranking label slot")
        return values(slots)

    write("ranking_chunks.inc", [
        'template<unsigned N> struct RankingLabels {',
        '    std::array<unsigned,N> physicalSlots;',
        '    constexpr unsigned operator[](unsigned i) const {return physicalSlots[i]+1;} };',
        'inline constexpr RankingLabels<9> courseChunks{' + ranking_labels(0x0C2C2E48, 9) + '};',
        'inline constexpr RankingLabels<18> routeChunks{' + ranking_labels(0x0C2C2E6C, 18) + '};'])
    # The renderer inserts the export-only snow slot at zero when loading
    # this bank, so convert the original chunk index at point of lookup.
    write("start_courses.inc", [
        'constexpr std::array<int,9> regionalCourseChunks' + values(words(0x0C2FCFF4, 9)) + ';',
        'struct CourseChunks { constexpr unsigned size() const {return 9;}',
        '    constexpr int operator[](unsigned i) const {return regionalCourseChunks[i]+1;} };',
        'constexpr CourseChunks courseChunk{};'])


if __name__ == "__main__":
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("program", type=Path)
    args.add_argument("destination", type=Path)
    options = args.parse_args()
    generate(options.program, options.destination)
