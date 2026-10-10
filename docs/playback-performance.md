# Playback performance checks

Keep the native DXVA2 sample lifetime, GPU query draining, flush handshake and
lock order unchanged during control/UI optimizations. A fast API response does
not establish that a new frame has appeared on screen.

## Reproducible measurements

Use Python 3 on Windows and a separate portable build with its own INI, muted
audio, EVR Custom and repeat enabled. Disable update checks in that INI. Do not
benchmark against a shared registry configuration. Keep the same renderer,
window state, decoder, files and settings for both revisions. Stop compilation
and other heavy work before comparing CPU usage. Warm local-file measurements
do not cover network sources or cold disk access.

```powershell
python .\contrib\playback_benchmark.py C:\test\mpc-hc64.exe C:\media\long.mp4 C:\media\h264.mp4 --rounds 5 --output C:\test\measurements.json
```

The script launches and closes its own instance. It measures opening through
`CMD_STATE=MLS_LOADED`, relative seek command completion, and graph-clock
advancement after a seek. It also records process CPU during playing and pause,
as a percentage of one logical CPU. These include API/polling overhead and are
**not** measurements of first-frame presentation or frame smoothness. Use the
renderer statistics or a presentation trace for those. JSON includes raw samples
so startup outliers and variation are visible.

For coarse internal operation timings, set `DebugLogMask=1` under `[Settings]`
in the portable INI before launching. `player.log` then includes `PERF` records
for seeking, graph opening/closing, file rendering and statistics setup, in ms
from QueryPerformanceCounter. Disable logging for final throughput/CPU runs.
Timing scopes add no renderer locks and are not used on renderer threads.

## Regression checks

For both software AV1 and native DXVA2 H.264, check forward/backward seeks,
paused seeks, resume, repeated playback across EOF, and alternating files.
Also exercise a burst of seeks, play/pause and close while commands are pending.
Verify the final requested position and advancing playback; require bounded
command completion. Inspect retained samples on the native decoder if sample
lifetime changes, and visually check the old staircase artifact.

## Initial baseline (2026-10-10)

Revision `f4c0f0b0c`, EVR Custom, minimized muted portable instance, three
opens and twelve seeks per file, warm local files:

| File | Open median | Seek command median | Clock advancement median |
| --- | ---: | ---: | ---: |
| AV1 720p30, 4138.84 s | 450 ms | 25.4 ms | 129 ms |
| Native H.264 720p30, 4 s | 417 ms | 17.9 ms | 85.5 ms |

Compilation overlapped part of this initial run, so its CPU readings must not
be used to claim a CPU reduction. Repeat CPU comparisons without a build running.

## Seek scheduling

Pending seeks now replace their target without re-arming the existing timer.
The timer callback clears the old request before servicing it, allowing an
early timer delivery to schedule a new wait normally. The existing 16/40/100 ms
limits and nonblocking EVR preroll checks are unchanged. Seeking also avoids an
extra throttled SMTC timeline call immediately before the forced update.

Release x64 validation: long AV1 and native H.264 forward/backward seeks,
three paused seeks and three EOF repeats per decoder passed. A burst of ninety
paused seeks retained the final 1.7 s target and resumed playback successfully.
