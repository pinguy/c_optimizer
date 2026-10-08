"""Catch the original upside-down HUD digit bug before a release build."""
from pathlib import Path
import re
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "examples" / "TETHER9.c"

class DigitOrientation(unittest.TestCase):
    def test_numeric_glyphs_are_top_first(self):
        s = SOURCE.read_text()
        m = re.search(r'static const unsigned char font\[\]\[5\]\s*=\s*\{(.*?)\};', s, re.S)
        self.assertIsNotNone(m)
        glyphs = re.findall(r'\{([^{}]+)\}', m.group(1))
        self.assertEqual(43, len(glyphs))
        # Independently specified pixel artwork, top row first. These glyphs
        # were stored bottom-first in the initial game despite a top-first renderer.
        expected = {
            1: ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
            2: [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
            4: ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
            7: ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
        }
        for digit, rows in expected.items():
            columns = [int(value.strip(),0) for value in glyphs[26+digit].split(',')]
            actual = [''.join('#' if (c>>j)&1 else '.' for c in columns) for j in range(7)]
            self.assertEqual(rows, actual, f"digit {digit} is mirrored vertically")

if __name__ == '__main__':
    unittest.main()
