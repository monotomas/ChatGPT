# G3 phase-gap causal trace

This is a diagnostic reproduction on one synthetic open-six-string repluck fixture. It is not a musical pass or production-readiness result. No algorithm, threshold, frequency, conditioning, renderer, or production-source edits were made. The private copy adds logging only. The driver selects only kind=1, phase=0, newGain=1, dynamics=1 from the corrected v2 renderer.

## Finding

The held FREE joint fit does observe the G3 phase change. The root advances an extra +2.284083 rad across clocks 5056–5760 after subtracting the expected oscillator rotation, close to the renderer's stable +2.233811 rad root change. The loss occurs in the reliability/state path and in the requirement for two harmonics to cross their thresholds in the same frame.

All six held notes (40;45;50;55;59;64) remain fixed over the entire traced interval, clocks 4576–6720 (0.381333–0.560000 s). Warmup remains 8, elapsed remains 32, and neither clock nor elapsed resets occur. Candidate/held-set changes do not explain the miss.

The root's new excitation is nearly opposite the old root (+2.863880 rad relative phase). The raw renderer's nearest cancellation retains 7.516% of old root power at 0.422437880 s; it does not reach zero. Its stable root retains 21.6506% of old power. The fitted trailing-window coefficient falls farther during the transition: from power 1.2700651e-4 at clock5024 to 2.1623211e-6 at5312. Global joint-fit residual variance also rises during the staggered replucks, peaking at1.0025158e-4 at5248 versus2.5548660e-7 at4800. This variance is model residual per degree of freedom, not a measured physical background-noise floor.

## Exact root gap

The full joint fit uses six held notes ×16 columns (dimension96). Root full-joint conditional variances remain0.00410522670493 (sine) and0.00413201384739 (cosine), yielding conditioning1.05762443949, well below8. Pairwise maximum coherence remains0.206121288633, below.80. Both gates pass. Failure is specifically power >9×noisePower:

| Clock / seconds | Power | Noise power | Power / noise | Reliable / old valid | State branch |
|---|---:|---:|---:|---|---|
|5248 / .437333|9.864582e-6|8.257964e-7|11.9455|1 /1|DEPARTURE|
|5280 / .440000|2.760176e-6|5.772096e-7|4.78193|0 /1|CLEAR|
|5312 / .442667|2.162321e-6|4.654931e-7|4.64523|0 /0|CLEAR|
|5344 / .445333|3.860318e-6|4.935888e-7|7.82092|0 /0|CLEAR|
|5376 / .448000|5.826012e-6|4.930497e-7|11.8163|1 /0|CLEAR|
|5408 / .450667|1.265977e-5|3.376735e-7|37.4912|1 /1|DEPARTURE|

At5280 the old accumulated departure0.8036860330 is set to0, and trend0.000228988602 rad/sample is set to0. Phase is overwritten on every unreliable observation. At5376 the root is reliable again, but oldValid=false, so its advance is also discarded and the reference is replaced once more. Across those four observations, the measured extra root phase advance is+0.7313329901 rad. There is no comparison spanning the last reliable pre-gap root phase and the first reliable post-gap root phase.

A concrete5280 row has beta(sin,cos)=(-0.000701821095,-0.001505863011), centered angle=-0.9191914511, previous angle=1.9002324455, omega=0.102624165160 rad/sample anddt32. Expected oscillator advance=3.2839732851 rad; actual wrapped advance=-2.8194238966 rad; residual difference=+0.1797881254 rad. The old trend predicts another+0.00732763525 rad. The gap gate clears state before this can be accumulated.

## Why no two-harmonic event survives

Harmonics3,5,6 fail pairwise coherence throughout: respectively.998681777 (D3 harmonic4),.828434925 (B3 harmonic4),.994733362 (D3 harmonic8). Harmonic3 also has full-joint conditioning11.9352741 >8. Harmonics7 and8 lose reliability at4928 (.410667 s), before the G3 repick, as the other strings begin replucking and global fit residual rises. Harmonic4 loses reliability at5088 and then remains unreliable from5216 through5632. Their phase references are cleared as well.

Harmonic2 keeps its phase reference. Its measured phase change over5056–5760 is-0.931323059 rad versus the renderer's stable-0.874548 rad. Its largest leaky departure is0.533239731 rad at5376, still below that frame's uncertainty-adjusted threshold0.678562. Root and harmonic2 eventually cross at different frames:

|Clock / seconds|Root abs departure / threshold|H2 abs departure / threshold|Supported / changed|
|---|---:|---:|---|
|5536 / .461333|.728261 /.590449|.492198 /.529902|2 /1|
|5568 / .464000|.697295 /.594431|.485310 /.517048|2 /1|
|5600 / .466667|.498420 /.609620|.523223 /.508550|2 /1|

These are the only frames with any changed harmonic in the traced interval. The code requires supported>=2 AND changed>=2 in the same frame. It never receives that evidence for G3; lastEvent stays-1000000 and sourceExcitation stays0.

Trend absorption exists but is secondary to the explicit gap and same-frame evidence failure. Root trend predicts only.0073276 rad per hop before the gap, resets to0, then reaches.0338116 rad per hop later. H2 predicts about-.00299387 rad per hop during most of the transition. Departures also decay with the existing40ms constant.

NULLexplained is false from4832 (.402667 s) through5728 (.477333 s), including the root reliability gap and all three one-harmonic changed frames. Thus the NULL guard does not suppress an otherwise eligible two-harmonic G3 event during that interval. At5760 (.480000 s), null relative residual.00134761083 falls below4×prior precision (.00184504902). The TREND branch then zeros surviving departures and learns the now-stable continuation. Since its prior coefficients are replaced every frame, it no longer compares against the old pre-repick wave.

## Reproduction and retained evidence

The frozen input Source tree and its post-run hashes are identical (`source-before.sha256.json`, `source-after.sha256.json`). Original TimeDomainPursuit.cpp SHA256: bedd49a8abf0919e81f80cd777a25280171dbd1097faa0978f070ca75c6c8d3f. The private instrumented source SHA256 is f465d980cf43871c115ab32bc875de08baa8dbef0b9c02e50c1a4aee576b8c3f; exact additions are in instrumentation.diff.

`compile-command.json` contains the actual taskset-c3 g++ command (-std=c++17 -O2 -DNDEBUG -DFLYSNAPPER_RESEARCH_TRACE_NULL, all eight Pitch cpp files except MpeOutputRouter.cpp, plus driver). `compile-result.json` records exit0,6.1322s. `compile.stdout`/`compile.stderr` retain compiler output. `run-result.json` records exit0,1.0465s. These durations include logging and environment load and are not a performance claim.

`raw-midi.csv` reproduces first NoteOns45,40,55,59,50,64, then only an E2 off/on pair atsample23039. No G3 repluck is emitted. `trace.log` is the complete raw stderr. `phase.csv` contains every G3 harmonic at every observation from0.38–0.56s; columns include beta sin/cos, centered current/previous phase, oscillator and trend advances, innovation/departure, power/noise variance, full-joint conditional diagonals/conditioning, pairwise coherence, reliability, oldValid, warmup, and NULLexplained. `state.csv`, `held.csv`, `decision.csv`, `trace-columns.json`, `summary.json`, and `renderer-and-coherence-math.json` provide structured evidence.

No fix has been proposed or tested by this trace task.
