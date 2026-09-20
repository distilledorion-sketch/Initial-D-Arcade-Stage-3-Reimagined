"""Search a byte blob for the 256-entry palette that decodes an 8bpp texture
into a natural image. Objective test, not an eyeball judgement: the correct
palette makes horizontally adjacent pixels similar in colour while still
spanning a wide gamut; a wrong palette makes noise, and a flat run of bytes is
rejected by the spread normalisation."""
import numpy as np

def lanes(u16, fmt):
    if fmt == 'rgb565':
        return ((u16 >> 11) & 31).astype(np.int32) * 8, ((u16 >> 5) & 63).astype(np.int32) * 4, (u16 & 31).astype(np.int32) * 8
    if fmt == 'argb1555':
        return ((u16 >> 10) & 31).astype(np.int32) * 8, ((u16 >> 5) & 31).astype(np.int32) * 8, (u16 & 31).astype(np.int32) * 8
    if fmt == 'argb4444':
        return ((u16 >> 8) & 15).astype(np.int32) * 17, ((u16 >> 4) & 15).astype(np.int32) * 17, (u16 & 15).astype(np.int32) * 17
    raise ValueError(fmt)

def scan(blob_u16, pairs, weights, fmt, entries=256):
    R, G, B = lanes(blob_u16, fmt)
    n = len(blob_u16) - entries
    cost = np.zeros(n, dtype=np.float64)
    for (i, j), w in zip(pairs, weights):
        for ch in (R, G, B):
            cost += w * np.abs(ch[i:i + n].astype(np.int32) - ch[j:j + n].astype(np.int32))
    cost /= weights.sum()
    # spread of each 256-entry window, via cumulative sums
    spread = np.zeros(n)
    for ch in (R, G, B):
        c1 = np.concatenate(([0], np.cumsum(ch.astype(np.float64))))
        c2 = np.concatenate(([0], np.cumsum(ch.astype(np.float64) ** 2)))
        m = (c1[entries:entries + n] - c1[:n]) / entries
        m2 = (c2[entries:entries + n] - c2[:n]) / entries
        spread += np.sqrt(np.maximum(m2 - m * m, 0))
    return cost, spread
