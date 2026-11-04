# Historical exploratory reports

These files preserve prior observations, including failed hypotheses and
measurement defects. They are not output from the corrected archive runner.
Use [the archive README](../README.md), `../run.py` and `../corrected/` for the
reproducible, explicitly scored results.

`results-primed-trim.csv` modified the reference onset inside its three-gain
loop. Only the first gain was trimmed. Its aggregate and later-gain latency
values must not be presented as simultaneous-onset results. The corrected
runner copies immutable inventory intervals for every gain.

Rounded semitone scores can count 50 Hz interference as a correct G1/MIDI31
estimate. Historical `first_accepted_ms` also did not require correct pitch,
and used an ambiguous `-.083` sentinel. The corrected CSV separates first any
admission from first correct admission, uses `-1` for absence, and makes
25-cent scoring primary. The historical manifest describes earlier exploratory
sources; the corrected manifest hashes the actual archived implementation.
