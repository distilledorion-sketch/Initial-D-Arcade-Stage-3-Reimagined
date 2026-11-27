"""Verified program layouts for the two supported Stage 3 discs."""
from dataclasses import dataclass
import hashlib


ENGLISH_SHA256 = "efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335"
JAPANESE_SHA256 = "62e23a4213a65b677ad9ba46a2659ad5777bf7e87485bd6f8021146197821593"
BASE = 0x0C020000


@dataclass(frozen=True)
class CarLayout:
    roster: int
    folders: int
    presets: int
    slot_maps: int
    menu_order: int
    rivals: int
    palettes: int
    headlight_phases: int


@dataclass(frozen=True)
class NameLayout:
    glyphs: int
    keyboard: int
    substitutions: int
    start_names: int
    default_ids: int
    default_length: int
    animation: int


@dataclass(frozen=True)
class ProgramLayout:
    sha256: str
    vehicle_table_shift: int
    course_paths: int
    tuning: int
    engine_control_shift: int
    engine_parameters_shift: int
    music_cues: int
    sound_banks: int
    loading_palette_ids: int
    palettes: int
    cars: CarLayout
    portraits: int
    names: int
    player_name: int
    name_entry: NameLayout

    @property
    def menu_resource_tables(self):
        if self == JAPANESE:
            return ((0x0C2C1584, "pol"), (0x0C2C1BB4, "tex"))
        return ((0x0C2A9648, "pol"), (0x0C2A9C78, "tex"))


ENGLISH = ProgramLayout(ENGLISH_SHA256, 0, 0x0C2EFF40, 0x0C30ECC0, 0, 0, 0x0C31DF04, 0x0C31ED30,
                        0x0C28B8F8, 0x0C33B898,
                        CarLayout(0x0C2EF384, 0x0C2EF410, 0x0C2F4758, 0x0C33B250,
                                  0x0C2AAE80, 0x0C31D8A8, 0x0C33B1A0, 0x0C298320),
                        0x0C2FBB24, 0x0C33B684, 0x0C33B88C,
                        NameLayout(0x0C268D54, 0x0C2690C8, 0x0C31BCC8, 0x0C33B780,
                                   0x0C298958, 6, 0x0C31EE90))
JAPANESE = ProgramLayout(JAPANESE_SHA256, 0x77B4, 0x0C2F03D8, 0x0C30F36C, 0x484, 0x6CD4, 0x0C329788, 0x0C32A5B4,
                         0x0C293148, 0x0C347270,
                         CarLayout(0x0C2EF81C, 0x0C2EF8A8, 0x0C2F4BF0, 0x0C346C28,
                                   0x0C2C2DBC, 0x0C32912C, 0x0C346B78, 0x0C29FBB0),
                         0x0C2FBFA8, 0x0C34705C, 0x0C347264,
                         NameLayout(0x0C2704F4, 0x0C270868, 0x0C32754C, 0x0C347158,
                                    0x0C2A01F8, 5, 0x0C32A714))


def identify(image):
    digest = hashlib.sha256(image).hexdigest()
    for layout in (ENGLISH, JAPANESE):
        if digest == layout.sha256:
            return layout
    raise ValueError("A verified GDS-0033 or GDS-0032C program is required.")
