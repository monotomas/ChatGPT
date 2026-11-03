# MIDI core CPU measurement, 3 November 2026

This measures CPU execution of the numeric MIDI core, **not pitch detection or
audio latency**. It cannot substantiate the requested guitar-to-instrument
5/10 ms limits. No driver, converter, VST3 adapter, Live routing, receiver
instrument or audio-output work is included.

Environment: Linux x86_64 shared cloud VM, AMD EPYC 9V74 exposed by the hypervisor,
GCC 14.2.0, `-O2 -DNDEBUG`, five visible logical CPUs, cgroup quota four CPU
seconds per second, 32 GiB memory limit. Other builds/tasks were active on the
machine. This is not a dedicated realtime workstation; maxima include operating
system scheduling and VM contention. No thread priority, affinity, interpolation
or removal of outliers was applied.

`Tools/CoreBenchmark.cpp` preinitialises lookup tables and measures each block
using `steady_clock` around the kernel. Each case has 200 warm-up blocks and
2,000 measured blocks at 48 kHz. The kernel always reads/applies settings,
refreshes chord recognition and produces a packed display snapshot. Every 32
blocks it also changes the held chord input. Loads:

- **Idle:** no performer notes.
- **Musical:** two simultaneous input sources, four note events per block.
- **Dense:** 2,048 note events per block, two rounds of 512 simultaneous input
  bindings followed by all corresponding Note Offs. This exercises the full
  binding capacity without using overflow to shorten the work.

All kernels finished with no active voices, no overflow and no latched fault.
Percentiles use sorted measured wall times, without discarding stalls.

| Load | Buffer | Deadline µs | Median µs | P95 µs | P99 µs | Maximum µs | Misses / 2,000 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Idle | 32 | 666.667 | 0.191 | 0.291 | 0.331 | 52.306 | 0 |
| Idle | 64 | 1,333.333 | 0.190 | 0.290 | 0.331 | 0.791 | 0 |
| Idle | 128 | 2,666.667 | 0.190 | 0.191 | 0.331 | 11.526 | 0 |
| Musical | 32 | 666.667 | 0.681 | 0.691 | 0.821 | 12.889 | 0 |
| Musical | 64 | 1,333.333 | 0.681 | 0.691 | 0.822 | 14.130 | 0 |
| Musical | 128 | 2,666.667 | 0.681 | 0.701 | 0.831 | 234.123 | 0 |
| Dense | 32 | 666.667 | 398.605 | 659.537 | 799.272 | 2,268.788 | 87 |
| Dense | 64 | 1,333.333 | 609.725 | 699.045 | 793.354 | 1,979.534 | 6 |
| Dense | 128 | 2,666.667 | 531.860 | 675.941 | 787.655 | 3,785.042 | 1 |

The dense case exceeded block deadlines in this run. There is no hard realtime
guarantee for the maximum event load, and the wrapper/host will add work beyond
this kernel. These are observations from one run, not a measured universal
upper bound. Live latency and guitar accuracy remain unmeasured.

These results were refreshed after the final hold-pedal panic correction. The
raw output is in `validation/core-benchmark.csv`. Separate final ASan/UBSan/LSan
execution passed 6 suites and 2,104,154 assertions; its output is retained in
`validation/core-sanitizers.txt`.

Reproduce from the project root (any suitable C++17 compiler may be used):

```sh
g++ -std=c++17 -O2 -DNDEBUG -I Source Source/Core/MusicData.cpp \
    Source/Core/MidiEngine.cpp Tools/CoreBenchmark.cpp -o /tmp/flysnapper-core-benchmark
/tmp/flysnapper-core-benchmark 2000
```

The standalone core test separately checks 10,000 representative processing
iterations with global C++ allocation counting enabled. That check sees zero
`new`/`new[]` calls in the engine after construction. It does not claim zero
allocations in the full VST3 wrapper or host-owned MIDI buffers.
