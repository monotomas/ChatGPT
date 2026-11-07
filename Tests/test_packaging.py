# SPDX-License-Identifier: AGPL-3.0-or-later
"""Source packaging integrity tests, without requiring JUCE or a compiler."""
import hashlib
import importlib.util
from pathlib import Path
import stat
import tempfile
import unittest
from unittest import mock
from zipfile import ZipFile

SCRIPT = Path(__file__).resolve().parents[1] / "Tools/package_source.py"
spec = importlib.util.spec_from_file_location("flysnapper_package_source", SCRIPT)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "source"
        self.root.mkdir()
        for name in package.REQUIRED_FILES:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture " + name + "\n", encoding="utf-8")
        self.output = Path(self.temp.name) / "delivery.zip"

    def test_complete_safe_extract_and_all_hashes(self):
        self.assertEqual(package.VERSION,"0.4.3")
        self.assertEqual(package.PACKAGE_ROOT,"Flysnapper-0.4.3")
        self.assertIn("docs/RELEASE_0.4.3.md",package.REQUIRED_FILES)
        extra = self.root / "docs/handover/Original.zip"
        extra.parent.mkdir(parents=True)
        extra.write_bytes(b"unchanged original handover")
        for name in ("deps/secret.txt", "build/binary.exe", "dist/old.zip", "logs/build.txt",
                     ".git/config", "Tests/__pycache__/x.pyc", ".env", "private.txt"):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("excluded", encoding="utf-8")
        package.build_package(self.root, self.output)
        package.verify_package(self.output)
        with ZipFile(self.output) as archive:
            prefix = package.PACKAGE_ROOT + "/"
            inventory = {n[len(prefix):] for n in archive.namelist()}
            self.assertEqual(inventory, package.REQUIRED_FILES | {"docs/handover/Original.zip", "SHA256SUMS.txt"})
            destination = Path(self.temp.name) / "extracted"
            for info in archive.infolist():
                path = destination / info.filename
                self.assertTrue(path.resolve().is_relative_to(destination.resolve()))
                self.assertFalse(stat.S_ISLNK(info.external_attr >> 16))
                self.assertEqual(info.date_time, package.FIXED_TIMESTAMP)
            archive.extractall(destination)
            for line in (destination / prefix / "SHA256SUMS.txt").read_text().splitlines():
                expected, name = line.split("  ", 1)
                self.assertEqual(hashlib.sha256((destination / prefix / name).read_bytes()).hexdigest(), expected)

    def test_current_midi_evidence_is_included_without_historical_research_bloat(self):
        files={
            "docs/validation/old-guitar/archive.bin": b"historical retained only in Git",
            "docs/validation/old-summary.json": b"historical aggregate",
            "docs/research/old-model/notes.txt": b"historical research retained only in Git",
            "docs/validation/midi-only-2026-11-07/report.json": b"historical 50-chord MIDI proof",
            "docs/validation/midi-only-52-2026-11-07/report.json": b"historical 31-scale52-chord MIDI proof",
            "docs/validation/midi-only-38-scales-2026-11-07/report.json": b"historical provisional-factory 38-scale52-chord MIDI proof",
            "docs/validation/midi-factory-2026-11-07/report.json": b"historical zero-audio user-factory MIDI proof",
            "docs/validation/midi-instrument-2026-11-07/report.json": b"historical instrument-adapter MIDI proof",
            "docs/validation/midi-factory-final-2026-11-07/report.json": b"historical final-factory instrument-adapter MIDI proof",
            "docs/validation/midi-articulation-0.4.3/report.json": b"current legato and rearticulation instrument-adapter MIDI proof",
            "docs/factory-settings/2026-11-07-user-values/Flysnapper_factory_assignments_user.xlsx": b"historical original user workbook bytes",
            "docs/factory-settings/2026-11-07-user-values/assignments.json": b"historical original user assignments",
            "docs/factory-settings/2026-11-07-user-values-final/Flysnapper_factory_assignments_user.xlsx": b"current uploaded 52-row workbook bytes preserved",
            "docs/factory-settings/2026-11-07-user-values-final/assignments.json": b"unchanged current 52-chord assignments including both sharp9-thirteenth Chromatic overrides",
            "docs/handover/Original.zip": b"original user attachment",
        }
        for name,data in files.items():
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
        package.build_package(self.root,self.output)
        with ZipFile(self.output) as z:
            names=set(z.namelist());prefix=package.PACKAGE_ROOT+"/"
            for name in ("docs/validation/midi-articulation-0.4.3/report.json","docs/factory-settings/2026-11-07-user-values/Flysnapper_factory_assignments_user.xlsx","docs/factory-settings/2026-11-07-user-values/assignments.json","docs/factory-settings/2026-11-07-user-values-final/Flysnapper_factory_assignments_user.xlsx","docs/factory-settings/2026-11-07-user-values-final/assignments.json","docs/handover/Original.zip"):
                self.assertIn(prefix+name,names)
                self.assertEqual(z.read(prefix+name),files[name])
            for name in ("docs/validation/old-guitar/archive.bin","docs/validation/old-summary.json","docs/validation/midi-only-2026-11-07/report.json","docs/validation/midi-only-52-2026-11-07/report.json","docs/validation/midi-only-38-scales-2026-11-07/report.json","docs/validation/midi-factory-2026-11-07/report.json","docs/validation/midi-instrument-2026-11-07/report.json","docs/validation/midi-factory-final-2026-11-07/report.json","docs/research/old-model/notes.txt"):
                self.assertNotIn(prefix+name,names)
            for name in files:self.assertEqual((self.root/name).read_bytes(),files[name])

    def test_deterministic_repeated_packages(self):
        package.build_package(self.root, self.output)
        first = self.output.read_bytes()
        other = self.output.with_name("another.zip")
        package.build_package(self.root, other)
        self.assertEqual(first, other.read_bytes())

    def test_self_exclusion_inside_included_tree(self):
        output = self.root / "docs/delivery.zip"
        output.write_bytes(b"old source package")
        package.build_package(self.root, output)
        with ZipFile(output) as archive:
            self.assertNotIn(package.PACKAGE_ROOT + "/docs/delivery.zip", archive.namelist())

    def test_incomplete_source_fails_before_output(self):
        (self.root / "Source/Core/MidiEngine.cpp").unlink()
        with self.assertRaisesRegex(ValueError, "Incomplete source"):
            package.build_package(self.root, self.output)
        self.assertFalse(self.output.exists())

    def test_symlink_file_or_directory_rejected(self):
        target = Path(self.temp.name) / "outside.txt"
        target.write_text("private", encoding="utf-8")
        link = self.root / "docs/linked.txt"
        try:
            link.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest("symlinks unavailable for this test user/platform")
        with self.assertRaisesRegex(ValueError, "Symbolic links"):
            package.build_package(self.root, self.output)
        link.unlink()
        directory_link = self.root / "Source/external"
        directory_link.symlink_to(target.parent, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "Symbolic links"):
            package.build_package(self.root, self.output)

    def test_symlink_output_rejected(self):
        target = Path(self.temp.name) / "outside.zip"
        target.write_bytes(b"must survive")
        try:
            self.output.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest("symlinks unavailable for this test user/platform")
        with self.assertRaisesRegex(ValueError, "symbolic-link output"):
            package.build_package(self.root, self.output)
        self.assertEqual(target.read_bytes(), b"must survive")

    def test_case_collision_rejected_for_windows(self):
        (self.root / "Source/plugineditor.cpp").write_text("collision", encoding="utf-8")
        # On a case-insensitive Windows directory the write above names the
        # same physical file. Feed a case-colliding directory inventory to test
        # the portable collector rather than assume Linux filesystem behavior.
        walk = package.os.walk
        def case_colliding_inventory(directory, **options):
            for current, directories, names in walk(directory, **options):
                if Path(current).resolve() == (self.root / "Source").resolve():
                    names = [n for n in names if n.casefold() != "plugineditor.cpp"]
                    names += ["PluginEditor.cpp", "plugineditor.cpp"]
                yield current, directories, names
        with mock.patch.object(package.os, "walk", side_effect=case_colliding_inventory):
            with self.assertRaisesRegex(ValueError, "Case-colliding"):
                package.build_package(self.root, self.output)

    def test_verification_rejects_case_colliding_zip_members(self):
        package.build_package(self.root, self.output)
        with ZipFile(self.output) as source:
            members = {name: source.read(name) for name in source.namelist()}
        damaged = self.output.with_name("case-collision.zip")
        with ZipFile(damaged, "w") as archive:
            for name, data in members.items(): archive.writestr(name, data)
            archive.writestr(package.PACKAGE_ROOT + "/Source/plugineditor.cpp", b"collision")
        with self.assertRaisesRegex(ValueError, "Case-colliding"):
            package.verify_package(damaged)

    def test_verification_detects_changed_or_unlisted_members(self):
        package.build_package(self.root, self.output)
        with ZipFile(self.output) as archive:
            original = {name: archive.read(name) for name in archive.namelist()}
        damaged = self.output.with_name("damaged.zip")
        for changed_name, changed_data in ((package.PACKAGE_ROOT + "/Source/Core/MidiEngine.cpp", b"modified"),
                                           (package.PACKAGE_ROOT + "/extra.txt", b"unlisted")):
            with self.subTest(member=changed_name):
                entries = dict(original)
                entries[changed_name] = changed_data
                with ZipFile(damaged, "w") as archive:
                    for name, data in entries.items():
                        archive.writestr(name, data)
                with self.assertRaises(ValueError):
                    package.verify_package(damaged)

    def test_unsafe_names_rejected(self):
        for name in ("/absolute", "../escape", "dir/../escape", "dir\\escape", "line\nbreak", "dir/C:/escape", "dir/./name", "dir/CON.txt", "dir/trailing."):
            with self.subTest(name=name), self.assertRaises(ValueError):
                package._safe_name(name)


if __name__ == "__main__":
    unittest.main()
