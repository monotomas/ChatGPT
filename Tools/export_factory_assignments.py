#!/usr/bin/env python3
"""Export the requested 52-chord/38-scale worksheet, with named scale tones.

Developer helper; requires openpyxl. Users only need the generated XLSX.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = ROOT / 'docs/factory-settings/2026-11-07'
DESTINATION = ROOT / 'docs/factory-settings/2026-11-07-revision-3'
NOTES = ('C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B')
ADDITIONS = (
    ('major_thirteenth_sharp11', 'CMaj13#11', 'C D E F# G A B'),
    ('major_ninth_sharp11', 'CMaj9#11', 'C D E F# G B'),
)
HARMONIC_MAJOR = (0, 2, 4, 5, 7, 8, 11)
HARMONIC_MAJOR_NAMES = (
    'Harmonic Major 1',
    'Harmonic Major 2 (Dorian b5)',
    'Harmonic Major 3 (Phrygian b4)',
    'Harmonic Major 4 (Lydian b3)',
    'Harmonic Major 5 (Mixolydian b2)',
    'Harmonic Major 6 (Lydian Augmented #2)',
    'Harmonic Major 7 (Locrian bb7)',
)
# Preserve the scale-degree spelling, including Fb and Bbb. Their MIDI
# classes are checked independently against the rotated parent below.
HARMONIC_MAJOR_TONES = (
    'C D E F G Ab B',
    'C D Eb F Gb A Bb',
    'C Db Eb Fb G Ab Bb',
    'C D Eb F# G A B',
    'C Db E F G A Bb',
    'C D# E F# G# A B',
    'C Db Eb F Gb Ab Bbb',
)


def note_class(name: str) -> int:
    natural = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
    assert name and name[0] in natural and set(name[1:]) <= {'b', '#'}
    return (natural[name[0]] + name[1:].count('#') - name[1:].count('b')) % 12


def export(destination: Path) -> dict:
    from openpyxl import Workbook, load_workbook
    from openpyxl.comments import Comment
    from openpyxl.styles import Alignment, Font, PatternFill
    from openpyxl.workbook.defined_name import DefinedName
    from openpyxl.worksheet.datavalidation import DataValidation

    destination.mkdir(parents=True, exist_ok=True)
    catalog = json.loads((ORIGINAL / 'scale_catalog.json').read_text())['scales']
    assert len(catalog) == 31
    modes = []
    for mode, (name, tones) in enumerate(zip(HARMONIC_MAJOR_NAMES, HARMONIC_MAJOR_TONES)):
        semitones = [(HARMONIC_MAJOR[(mode + n) % 7] - HARMONIC_MAJOR[mode]) % 12 for n in range(7)]
        assert semitones == sorted(semitones)
        assert [note_class(note) for note in tones.split()] == semitones
        modes.append({'family': 'Harmonic Major', 'mode': mode + 1,
                      'id': 'harmonic_major' if mode == 0 else 'harmonic_major_mode' + str(mode + 1),
                      'name': name, 'semitones': semitones, 'pitch_classes_in_c': tones.split()})
    insert_at = next(i for i, scale in enumerate(catalog) if scale['family'] == 'Diminished')
    catalog[insert_at:insert_at] = modes
    count = len(catalog)
    assert count == 38
    for scale in catalog:
        scale.setdefault('pitch_classes_in_c', [NOTES[n] for n in scale['semitones']])
        scale['display_name'] = scale['name'] + ' (' + ' '.join(scale['pitch_classes_in_c']) + ')'
    by_name = {s['name']: s for s in catalog}
    assert len(by_name) == count and len({s['display_name'] for s in catalog}) == count
    rows = list(csv.DictReader(io.StringIO((ORIGINAL / 'chord_types.tsv').read_text()), delimiter='\t'))
    assert len(rows) == 50 and len({r['Chord ID'] for r in rows}) == 50
    original_ids = [r['Chord ID'] for r in rows]
    original_choices = [r['Your factory scale'] for r in rows]
    for chord_id, chord_name, tones in ADDITIONS:
        rows.append({'Chord ID': chord_id, 'Chord in C': chord_name, 'Chord tones in C': tones,
                     'Current factory scale': 'Lydian', 'Your factory scale': 'Lydian'})
    for row in rows:
        for column in ('Current factory scale', 'Your factory scale'):
            row[column] = by_name[row[column]]['display_name']

    workbook = Workbook()
    assignments = workbook.active
    assignments.title = 'Assignments'
    scales = workbook.create_sheet('Scales')
    instructions = workbook.create_sheet('Read me')
    headers = ('Chord ID', 'Chord in C', 'Chord tones in C', 'Current / provisional factory scale', 'Your factory scale')
    assignments.append(headers)
    for row in rows:
        assignments.append([row[c] for c in ('Chord ID', 'Chord in C', 'Chord tones in C', 'Current factory scale', 'Your factory scale')])
    scales.append(('Family', 'Mode', 'Scale name with tones in C', 'Scale ID', 'Semitones from root C', 'Pitch classes in C', 'Base scale name'))
    for scale in catalog:
        scales.append((scale['family'], scale['mode'], scale['display_name'], scale['id'],
                       ', '.join(map(str, scale['semitones'])), ' '.join(scale['pitch_classes_in_c']), scale['name']))
    guidance = [
        'Flysnapper factory assignments — 52 chord types / 38 scales, revision 3 — 7 November 2026',
        'Edit the yellow Your factory scale cells on Assignments, save and upload this XLSX in chat.',
        'Keep all 52 Chord IDs unchanged. The original 50 IDs and choices are preserved; the two requested types are appended.',
        'All 38 choices include their scale tones in parentheses, relative to C. The plugin may display names and tones separately.',
        'Harmonic Major and its seven modes are included, derived from MIDI intervals 0, 2, 4, 5, 7, 8, 11.',
        'Assignments apply transposed to each actual chord root, not permanently to C.',
        'The two new #11 types currently have provisional Lydian choices. You can replace these exactly like any other yellow cell.',
        'The new types are CMaj13#11 = C D E F# G A B and CMaj9#11 = C D E F# G B.',
        'Use the cell dropdown. The Scales sheet lists the same choices, stable Scale IDs, intervals and base names.',
        'Chord tones follow the nominal formula; recognition may permit selected omitted tones. This sheet does not redefine omissions.',
        'Harmonic Major modes use scale-degree spelling: Fb equals E, Bbb equals A, D# equals Eb and G# equals Ab in MIDI.',
        'This is an editing worksheet, not an installer or an Ableton compatibility test. Product validation is recorded separately.',
    ]
    for line in guidance:
        instructions.append((line,))

    header_fill = PatternFill('solid', fgColor='243746')
    body_fill = PatternFill('solid', fgColor='EDF3F7')
    choice_fill = PatternFill('solid', fgColor='FFF2CC')
    for sheet in workbook:
        sheet.freeze_panes = 'A2'
        sheet.sheet_view.zoomScale = 90
        for cell in sheet[1]:
            cell.font = Font(name='Calibri', size=11, color='FFFFFF', bold=True)
            cell.fill = header_fill
            cell.alignment = Alignment(vertical='center', wrap_text=True)
        sheet.row_dimensions[1].height = 30
        for row in sheet.iter_rows(min_row=2):
            for cell in row:
                cell.font = Font(name='Calibri', size=11)
                cell.fill = body_fill
                cell.alignment = Alignment(vertical='center', wrap_text=True)
    assignments.freeze_panes = 'D2'
    for column, width in {'A': 34, 'B': 18, 'C': 32, 'D': 72, 'E': 72}.items():
        assignments.column_dimensions[column].width = width
    assignments.auto_filter.ref = 'A1:E53'
    for row in range(2, 54):
        assignments.cell(row, 5).fill = choice_fill
        assignments.row_dimensions[row].height = 32
    for row in (52, 53):
        for column in (4, 5):
            assignments.cell(row, column).comment = Comment('New chord type requested by the user. Lydian is a provisional starting value; choose the final factory scale in column E.', 'Flysnapper')
    for column, width in {'A': 24, 'B': 9, 'C': 75, 'D': 30, 'E': 38, 'F': 40, 'G': 65}.items():
        scales.column_dimensions[column].width = width
    scale_last_row = count + 1
    scales.auto_filter.ref = f'A1:G{scale_last_row}'
    for row in range(2, scale_last_row + 1):
        scales.row_dimensions[row].height = 32
    instructions.column_dimensions['A'].width = 145
    for row in range(2, len(guidance) + 1):
        instructions.row_dimensions[row].height = 38
    scale_range = f"'Scales'!$C$2:$C${scale_last_row}"
    workbook.defined_names.add(DefinedName('ScaleChoices', attr_text=scale_range))
    validation = DataValidation(type='list', formula1='=ScaleChoices', allow_blank=False)
    validation.errorTitle = 'Choose a listed scale'
    validation.error = 'Select one of the 38 scale names with tones from the dropdown.'
    validation.showErrorMessage = True
    validation.errorStyle = 'stop'
    assignments.add_data_validation(validation)
    validation.add('E2:E53')
    workbook.properties.title = 'Flysnapper — 52 chord factory assignments'
    workbook.properties.description = 'Revision 3: 52 chords and 38 scales, including all seven Harmonic Major modes with scale tones.'
    filename = 'Flysnapper_factory_assignments_52_chords_38_scales.xlsx'
    workbook.save(destination / filename)

    output = io.StringIO(newline='')
    writer = csv.writer(output, delimiter='\t', lineterminator='\n')
    writer.writerow(headers)
    for row in assignments.iter_rows(min_row=2, values_only=True):
        writer.writerow(row)
    (destination / 'chord_types.tsv').write_text(output.getvalue(), encoding='utf-8')
    (destination / 'scale_catalog.json').write_text(json.dumps({'status': '52 chords / 38 scales; original31 names/IDs/formulas retained, seven Harmonic Major modes added', 'scales': catalog}, indent=2, ensure_ascii=False) + '\n')

    check = load_workbook(destination / filename)
    sheet = check['Assignments']
    assert sheet.max_row == 53 and sheet.max_column == 5
    assert [sheet.cell(r, 1).value for r in range(2, 52)] == original_ids
    assert [sheet.cell(r, 5).value for r in range(2, 52)] == [by_name[s]['display_name'] for s in original_choices]
    assert len({sheet.cell(r, 1).value for r in range(2, 54)}) == 52
    for row, (chord_id, chord_name, tones) in zip((52, 53), ADDITIONS):
        assert tuple(sheet.cell(row, c).value for c in (1, 2, 3)) == (chord_id, chord_name, tones)
    assert [check['Scales'].cell(r, 3).value for r in range(2, scale_last_row + 1)] == [s['display_name'] for s in catalog]
    assert check.defined_names['ScaleChoices'].attr_text == scale_range
    assert len(sheet.data_validations.dataValidation) == 1
    assert str(sheet.data_validations.dataValidation[0].sqref) == 'E2:E53'
    choices = {s['display_name'] for s in catalog}
    assert all(sheet.cell(r, c).value in choices for r in range(2, 54) for c in (4, 5))
    assert all(sheet.cell(r, 5).fill.fgColor.rgb == '00FFF2CC' for r in range(2, 54))
    check.close()
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    record = {'schema': 1, 'created_at_utc': datetime.now(timezone.utc).isoformat(),
              'chord_count': 52, 'scale_count': count, 'unchanged_original_chord_ids': 50,
              'unchanged_original_factory_scale_ids': 50, 'new_types': [x[0] for x in ADDITIONS],
              'harmonic_major_parent': list(HARMONIC_MAJOR),
              'harmonic_major_mode_ids': [s['id'] for s in modes],
              'scale_note_spellings_verified_against_midi_intervals': True,
              'provisional_new_type_scale_id': 'lydian', 'scale_display_labels_include_tones': True,
              'readback_verified': True, 'validation_range': 'E2:E53',
              'original_files_sha256': {n: sha(ORIGINAL / n) for n in ('chord_types.tsv', 'scale_catalog.json')},
              'exporter_sha256': sha(Path(__file__)),
              'files_sha256': {n: sha(destination / n) for n in (filename, 'chord_types.tsv', 'scale_catalog.json')},
              'scope': 'User editing worksheet. New chord types are user-requested extensions of the original 50-type library; this file is not a runtime/host validation claim.'}
    (destination / 'provenance.json').write_text(json.dumps(record, indent=2) + '\n')
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=DESTINATION)
    arguments = parser.parse_args()
    print(json.dumps(export(arguments.output), indent=2))
