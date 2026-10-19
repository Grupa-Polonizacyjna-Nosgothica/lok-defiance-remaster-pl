#!/usr/bin/env python3
"""Tests of tools/port_translation.py on a tiny synthetic repository: python3 -m unittest discover -s tools"""
import os, sys, tempfile, unittest

sys.path.insert(0, os.path.dirname(__file__))
import port_translation as p


def write(root, path, text):
    full = os.path.join(root, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8", newline="") as f:
        f.write(text)


class PortTranslationTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        orig = lambda lang, name, text: write(self.root, f"defiance-orig/raw-cine-text-{lang}/{name}", text)
        # a scene whose lines are re-used by a timed string, a crblock and a UI string
        orig("en", "01/scene.txt", "-- Hello there,\nmy old friend.\nWe meet again.\n")
        orig("pl", "01/scene.txt", "-- Witaj,\nmój stary druhu.\nZnów się spotykamy.\n")
        # a short item name that is a whole original file
        orig("en", "Inventory/item.txt", "Ornamental Shield.\n")
        orig("pl", "Inventory/item.txt", "Ozdobna tarcza.\n")
        # the Polish version merged two English lines into one
        orig("en", "01/merged.txt", "First line,\nsecond line,\nthird line,\nfourth line.\n")
        orig("pl", "01/merged.txt", "Pierwsza linia,\ndruga i trzecia linia,\nczwarta linia.\n")
        # the same bark translated in two different ways
        orig("en", "Misc/a.txt", "Get him!\n")
        orig("pl", "Misc/a.txt", "Łapać go!\n")
        orig("en", "Misc/b.txt", "Get him!\n")
        orig("pl", "Misc/b.txt", "Brać go!\n")
        # an original line that is also the whole text of a UI string must not be taken from a longer text
        orig("en", "01/talk.txt", "Close\nthe door.\n")
        orig("pl", "01/talk.txt", "Zamknij\ndrzwi.\n")

        self.strings = {
            "0000": "-- Hello there, \n40 0\nmy old friend. \n90 0\nWe meet again.",   # timed, exact
            "0001": "Ornamental Shield.",                                                  # whole-file item
            "0002": "Close",                                                               # only inside a longer text
            "0003": "Something that never occurred",
            "0004": "-- hello there, \n40 0\nMy Old Friend. \n90 0\nwe meet again.",       # case differences only
            "0005": "First line, \n30 0\nsecond line,",                                    # merged in the Polish original
            "0006": "Third line, \n30 0\nfourth line.",
        }
        for name, text in self.strings.items():
            write(self.root, f"strings/EN/{name}.txt", text)
            write(self.root, f"strings/PL/{name}.txt", text)
        self.cin = {"crblock_00000001_00.txt": "Get him!", "crblock_00000001_01.txt": "We meet again.",
                    "crblock_00000002_00.txt": "-- Hello there,"}
        for name, text in self.cin.items():
            write(self.root, f"cinematic/EN/{name}", text)
            write(self.root, f"cinematic/PL/{name}", text)

    def tearDown(self):
        self.tmp.cleanup()

    def run_tool(self, *args):
        argv = sys.argv
        sys.argv = ["port_translation.py", self.root, "--report-dir", os.path.join(self.root, "report"), *args]
        try:
            p.main()
        finally:
            sys.argv = argv

    def read(self, *parts):
        with open(os.path.join(self.root, *parts), encoding="utf-8") as f:
            return f.read()

    def pl(self, folder, name):
        return self.read(folder, "PL", name)

    def test_applies_exact_matches_and_keeps_the_structure(self):
        self.run_tool()
        self.assertEqual(self.pl("strings", "0000.txt"), "-- Witaj, \n40 0\nmój stary druhu. \n90 0\nZnów się spotykamy.")
        self.assertEqual(self.pl("strings", "0001.txt"), "Ozdobna tarcza.")
        self.assertEqual(self.pl("cinematic", "crblock_00000001_01.txt"), "Znów się spotykamy.")
        self.assertEqual(self.pl("cinematic", "crblock_00000002_00.txt"), "-- Witaj,")

    def test_case_and_punctuation_differences_are_applied_unless_strict(self):
        self.run_tool("--strict")
        self.assertEqual(self.pl("strings", "0004.txt"), self.strings["0004"])
        self.run_tool()
        self.assertIn("Witaj", self.pl("strings", "0004.txt"))

    def test_uncertain_cases_are_left_alone(self):
        self.run_tool()
        for name in ("0002", "0003", "0005"):
            self.assertEqual(self.pl("strings", f"{name}.txt"), self.strings[name], name)
        self.assertEqual(self.pl("cinematic", "crblock_00000001_00.txt"), "Get him!")  # two translations
        text = self.read("report", "close.md")
        self.assertIn("2 different translations", text)
        self.assertIn("longer original texts", text)
        self.assertIn("merges these lines", text)
        self.assertIn("Something that never occurred", self.read("report", "unmatched.md"))

    def test_lines_after_a_merge_are_paired_by_length(self):
        self.run_tool()
        self.assertEqual(self.pl("strings", "0006.txt"), self.strings["0006"])  # its first line is merged with the second one
        original = [o for o in p.load_originals(self.root) if o.name == "01/merged.txt"][0]
        self.assertEqual(original.pl_for[3], "czwarta linia.")
        self.assertIsNone(original.pl_for[1])

    def test_translated_files_are_not_overwritten(self):
        write(self.root, "strings/PL/0001.txt", "Moja wersja")
        self.run_tool()
        self.assertEqual(self.pl("strings", "0001.txt"), "Moja wersja")
        self.run_tool("--force")
        self.assertEqual(self.pl("strings", "0001.txt"), "Ozdobna tarcza.")

    def test_dry_run_changes_nothing(self):
        self.run_tool("--dry-run")
        self.assertEqual(self.pl("strings", "0000.txt"), self.strings["0000"])
        self.assertTrue(os.path.exists(os.path.join(self.root, "report", "applied.tsv")))


if __name__ == "__main__":
    unittest.main()
