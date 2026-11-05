# Third native attempt:14/15 groups passed, quiet-G1 regression still fails

This run used the integrated post-original-conditioning dynamics topology.
The old unchanged mono BendProcessor suite is restored; all other mandatory
native groups pass. DynamicsProcessor completed70 bounded cases, including
successful learned-settings resume/correction and manual/reset invalidation,
then failed the first sealed quiet-G1/gain1 **mono-notes** case. The broad
corpus's mono cases use bending and cannot substitute for this plain-mode
coverage. No acceptance is granted by this failed checkpoint.

The raw first note31 starts26367, then shifts to43 at40191 with later43
restarts. The physical source reference ends74400; actual final Off is76159.
Full MIDI/pitch bytes, failed partial JSON, CTestXML/logs and screenshots are
preserved. The test was not relaxed or changed into a pass.

The source snapshot was copied after the run while the parent froze all
inputs; every stored byte matches the actual before-compile fingerprint.
Runtime binaries are excluded. The 49-input snapshot intentionally retains
the six canonical quiet-mains WAVs included in its before-compile map. Their
bytes match the unchanged original sealed fixtures; these copies bind the
exact tested input and do not count as new recordings or independent data.
Later fixes require a new complete run with fresh raw evidence.
