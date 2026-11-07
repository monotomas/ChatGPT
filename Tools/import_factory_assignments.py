#!/usr/bin/env python3
"""Validate and preserve a user's factory worksheet as data, without rewriting it.

Developer helper; requires openpyxl. Does not change runtime source.
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
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'docs/factory-settings/2026-11-07-revision-3/scale_catalog.json'
TEMPLATE = ROOT / 'docs/factory-settings/2026-11-07-revision-3/chord_types.tsv'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(workbook: Path, output: Path) -> dict:
    from openpyxl import load_workbook

    require(not (output / 'assignments.json').exists(),
            'An import record already exists; use a new output directory to preserve its baseline')
    data = workbook.read_bytes()
    catalog = json.loads(CATALOG.read_text())['scales']
    labels = {scale['display_name']: scale for scale in catalog}
    require(len(labels) == 38, 'Expected 38 unique known scale choices')
    original = {r['Chord ID']: r for r in csv.DictReader(io.StringIO(TEMPLATE.read_text()), delimiter='\t')}
    require(len(original) == 52, 'Expected 52 template chord IDs')
    source = (ROOT / 'Source/Core/MusicData.cpp').read_text()
    type_section = source.split('constexpr std::array<ChordType,chordTypeCount> types{{', 1)[1].split('}};', 1)[0]
    types = [(chord, suffix, int(index)) for chord, suffix, index in
             re.findall(r'\{"([^"\n]+)","([^"\n]*)",(\d+)\}', type_section)]
    scale_section = source.split('constexpr std::array<ScaleDefinition,scaleCount> scales{{', 1)[1].split('}};', 1)[0]
    scale_ids = re.findall(r'\{"([^"\n]+)","[^"\n]+",', scale_section)
    require(len(types) == 52 and len(scale_ids) == 38, 'Runtime catalog cardinality differs')
    require({t[0] for t in types} == original.keys(), 'Runtime and worksheet chord IDs differ')
    indices = {scale_id: index for index, scale_id in enumerate(scale_ids)}

    book = load_workbook(workbook, read_only=True, data_only=False, keep_links=False)
    try:
        require({'Assignments', 'Scales'} <= set(book.sheetnames), 'Missing required worksheets')
        assignments = book['Assignments']
        require(assignments.cell(1, 1).value == 'Chord ID' and assignments.cell(1, 5).value == 'Your factory scale',
                'Expected chord IDs in column A and selected values in column E')
        selected = {}
        changed_d = []
        for row_number, row in enumerate(assignments.iter_rows(min_row=2, values_only=True), 2):
            if not any(value is not None for value in row):
                continue
            require(len(row) >= 5, f'Incomplete row {row_number}')
            chord_id, name, tones, previous, choice = row[:5]
            require(isinstance(chord_id, str) and chord_id in original, f'Unknown chord ID on row {row_number}')
            require(chord_id not in selected, f'Duplicate chord ID {chord_id}')
            reference = original[chord_id]
            require(name == reference['Chord in C'] and tones == reference['Chord tones in C'],
                    f'Chord metadata changed for {chord_id}; no recognition changes inferred from worksheet')
            require(isinstance(choice, str) and choice in labels,
                    f'Unknown, blank or formula-based scale choice for {chord_id}')
            if previous != reference['Current / provisional factory scale']:
                changed_d.append(chord_id)
            scale = labels[choice]
            selected[chord_id] = {'source_excel_row': row_number, 'chord_id': chord_id,
                                  'chord_name': name, 'scale_id': scale['id'],
                                  'scale_index': indices[scale['id']], 'scale_name': scale['name'],
                                  'selected_label': choice}
        require(selected.keys() == original.keys(), 'Missing chord IDs')
        require(not changed_d, 'Reference column D changed; selected choices must be explicit in column E')

        sheet_scales = {}
        for row in book['Scales'].iter_rows(min_row=2, values_only=True):
            if not any(value is not None for value in row):
                continue
            require(len(row) >= 7, 'Incomplete scale catalog row')
            family, mode, display, scale_id, semitones, tones, name = row[:7]
            require(scale_id in indices and scale_id not in sheet_scales, 'Unknown or duplicate scale ID in Scales')
            scale = next(s for s in catalog if s['id'] == scale_id)
            require(display == scale['display_name'] and name == scale['name']
                    and family == scale['family'] and mode == scale['mode']
                    and semitones == ', '.join(map(str, scale['semitones']))
                    and tones == ' '.join(scale['pitch_classes_in_c']),
                    f'Scale definition changed for {scale_id}; no scale changes inferred from worksheet')
            sheet_scales[scale_id] = display
        require(len(sheet_scales) == 38, 'Incomplete Scales sheet')
    finally:
        book.close()

    ordered = []
    for chord_id, suffix, old_index in types:
        row = dict(selected[chord_id])
        row['previous_scale_index'] = old_index
        row['previous_scale_id'] = scale_ids[old_index]
        row['changed'] = row['scale_index'] != old_index
        ordered.append(row)
    output.mkdir(parents=True, exist_ok=True)
    preserved = output / 'Flysnapper_factory_assignments_user.xlsx'
    require(not preserved.exists() or preserved.read_bytes() == data, 'Refusing to replace a different preserved upload')
    preserved.write_bytes(data)
    require(preserved.read_bytes() == data, 'Preserved upload bytes differ')
    sha = lambda b: hashlib.sha256(b).hexdigest()
    report = {'schema': 1, 'status': 'passed', 'observed_at_utc': datetime.now(timezone.utc).isoformat(),
              'upload_name': workbook.name, 'upload_bytes': len(data), 'upload_sha256': sha(data),
              'preserved_upload': preserved.name, 'selection_column': 'E / Your factory scale',
              'rows': 52, 'available_scales': 38, 'changed_assignments': sum(r['changed'] for r in ordered),
              'mapping_order': 'Runtime stable chord-ID order; Excel row order is not used as identity',
              'reference_column_D_unchanged': True, 'scale_definitions_unchanged': True,
              'formulas_evaluated': False, 'workbook_treated_as': 'Assignment data, not instructions',
              'catalog_sha256': sha(CATALOG.read_bytes()), 'template_tsv_sha256': sha(TEMPLATE.read_bytes()),
              'source_before_import_sha256': sha((ROOT / 'Source/Core/MusicData.cpp').read_bytes()),
              'importer_sha256': sha(Path(__file__).read_bytes()), 'assignments': ordered}
    (output / 'assignments.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    csv_data = io.StringIO(newline='')
    writer = csv.writer(csv_data, delimiter='\t', lineterminator='\n')
    writer.writerow(('Chord ID', 'Chord in C', 'Scale ID', 'Scale index', 'Your factory scale'))
    for row in ordered:
        writer.writerow((row['chord_id'], row['chord_name'], row['scale_id'], row['scale_index'], row['selected_label']))
    (output / 'assignments.tsv').write_text(csv_data.getvalue(), encoding='utf-8')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workbook', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = validate(args.workbook, args.output)
    print(json.dumps({key: value for key, value in result.items() if key != 'assignments'}, indent=2))
