# `tcp_server`: performance comparison of the master and `dev/alternative-tcp-server` implementations

Comparison of the `sg::net::tcp_server` on `master` (`c2cd9f3`) against the reworked version on
`dev/alternative-tcp-server` (`61886fe`).

## What actually differs

The two versions differ in where a session's callbacks run.

**master** invokes `OnSessionDataAvailable` inline, on the io thread, straight out of the session's
reader coroutine (`net/src/tcp_session.cpp`, reader loop). There is no handoff and no copy, but a
callback that blocks blocks an io thread, and with it every other session sharing that thread.

**branch** posts each session's callbacks to a per-session strand on a separate callback pool
(`net/src/tcp_server.cpp`, `make_session_callbacks`), and applies back-pressure: the reader parks on
`m_read_gate` until the `read_lease` handed to the callback is dropped
(`net/src/tcp_session.cpp`, reader loop and `read_lease::release`). A callback is therefore free to
block, and cannot stall the io threads — at the cost of two thread handoffs per read (out to the
callback pool, and back again to re-arm the reader).

The branch keeps one exception: `tcp_session::options_t::dont_read` sessions still run
`OnSessionCreated` and `OnSessionDataAvailable` inline on the io strand, because the user reads the
native handle and that may only be touched there (the `defer` flag in `make_session_callbacks`).
That path is measured separately below, and is what makes the cost of the handoff visible in
isolation.

## Method

The checked-in `cmake-build-container` tree is a **Debug** build (`-g`, no optimisation), which is
not a usable basis for performance numbers, so both versions were configured fresh as **Release**
(`-O3 -DNDEBUG`) with identical flags and the same compiler. `master` was built from a `git
worktree`, so the two trees differ only in the `tcp_server`/`tcp_session` sources.

A single benchmark source compiles against both public APIs (`launch()` vs `start()`, `no_threads`
vs `no_io_threads`+`no_callback_threads`, `wait_until_stopped()` vs `future_get_once()`) via
`if constexpr` shims. The load generator is plain blocking BSD sockets in the same process, so it is
byte-identical across the two builds and any difference is attributable to the server.

- 5 repetitions per scenario, `master` and `branch` interleaved run-by-run so machine drift hits both
  equally; **medians** reported.
- Loopback, `TCP_NODELAY` on the latency tests only.
- Session `timeout_msec` raised to 120 s so the idle timeout cannot fire mid-run.
- Listening ports kept below `/proc/sys/net/ipv4/ip_local_port_range` (32768) — inside that range the
  listener collides with a client's ephemeral port and `bind()` fails.

Environment: Intel i7-4980HQ (4 cores / 8 threads, 2.80 GHz), Linux 6.12.95, Debian clang 19.1.7.

Because the load generator shares the machine with the server, absolute figures are loopback numbers
on a busy 8-thread box and should be read as *relative* only.

## Results

### Throughput, trivial callback

| Scenario | master MB/s | branch MB/s | Δ |
|---|---:|---:|---:|
| 1 conn × 64KB, io=1 | 6484 | 3558 | **−45%** |
| 4 conn × 64KB, io=1 | 6059 | 4316 | −29% |
| 4 conn × 64KB, io=4/cb=4 | 10595 | 6749 | −36% |
| 8 conn × 16KB, io=4/cb=4 | 10336 | 8106 | −22% |
| 1 conn × 1KB, io=1 | 1663 | 1821 | **+9.5%** |
| 4 conn × 64KB, callback burns 50 µs | 3165 | 2164 | −32% |
| 4 conn × 64KB, callback sleeps 200 µs | 604 | 596 | −1.4% |

The single win (1 conn × 1KB) is a side effect of back-pressure: it lets the socket buffer coalesce,
so the branch serves the same bytes in 63k larger reads/s where master does 105k smaller ones.

### Echo round-trip latency

| Scenario | master p50 / p99 µs | branch p50 / p99 µs | Δ p50 |
|---|---:|---:|---:|
| 1 conn × 64B | 13.2 / 30.8 | 19.6 / 49.8 | **+48%** |
| 1 conn × 1KB | 12.0 / 29.7 | 22.7 / 52.0 | **+88%** |
| 4 conn × 64B, io=4/cb=4 | 27.2 / 70.4 | 45.4 / 95.3 | +67% |
| 4 conn × 64B, callback burns 50 µs | 75.6 / 110.1 | 97.2 / 168.1 | +28% |

### Connection churn

| Scenario | master conn/s | branch conn/s | Δ |
|---|---:|---:|---:|
| 4000 conns, sequential | 24460 | 22566 | −8% |
| 4000 conns, concurrency 4 | 42167 | 33807 | −20% |

Each accept in the branch additionally allocates a strand and a liveness token, and copies both into
three callback lambdas.

### Blocking callbacks, more sessions than io threads

16 connections, `io=2`, `cb=16`, callback sleeps 1 ms per read:

| | master | branch |
|---|---:|---:|
| MB/s | 90.7 | **736.3 (8.1×)** |
| callbacks/s | 1776 | 14412 |
| CPU-s per GB | 1.31 | 1.27 |

master is pinned at ~2000 callbacks/s — exactly `2 io threads × 1/ms`, because the callback occupies
the io thread. The branch reaches ~16 × 1/ms, the width of its callback pool. Reproducible to within
3% across all 5 repetitions. **This is the scenario the branch was designed for, and the margin is
large.**

### Head-of-line isolation

8 connections, one of which sleeps 2 ms per read; throughput of the *other seven*:

| Scenario | master MB/s | branch MB/s | Δ |
|---|---:|---:|---:|
| io=2, cb=8 | 5312 | 6438 | **+21%** |
| io=4, cb=8 | 10172 | 8047 | −21% |

Mixed: the branch protects the fast connections when io threads are scarce, but once master has
enough io threads to absorb the slow session its cheaper data path wins again.

## Why: cost of the handoff

Voluntary + involuntary context switches per callback (`getrusage`):

| Scenario | master | branch (deferred) |
|---|---:|---:|
| 1 conn × 64KB, io=1 | 0.12 | **1.96** |
| 4 conn × 64KB, io=4 | 0.20 | **2.09** |
| 8 conn × 16KB, io=4 | 0.15 | 1.03 |

Roughly two context switches per read, matching the two handoffs in the design: the `post` to the
callback strand, and the `dispatch` back to the io strand that cancels `m_read_gate` and resumes the
reader. Back-pressure means the extra thread buys no pipelining in exchange — the reader waits for
the callback either way.

### The `dont_read` path confirms it

`dont_read` is the one configuration in which the branch does *not* defer. Measured with the
callback performing its own `recv()` on the native handle, capped at `SO_RCVBUF` so it issues the
same number of syscalls the library's own reader would:

| Scenario | master (normal) | branch (deferred) | branch `dont_read` | master `dont_read` |
|---|---:|---:|---:|---:|
| 1 conn × 64KB, io=1 | 7063 | 4019 | 6588 | 6633 |
| 4 conn × 64KB, io=1 | 6535 | 4555 | 7097 | 6993 |
| 4 conn × 64KB, io=4/cb=4 | 11616 | 7510 | 12577 | 12822 |
| 8 conn × 16KB, io=4/cb=4 | 11166 | 8802 | 12261 | 12336 |

Two things fall out:

1. **master and branch are within ±2% of each other on `dont_read`** — statistically identical across
   all four scenarios. When the branch does not defer, it performs exactly like master. The
   regression is the handoff, not anything else in the rework.
2. Context switches per callback on the branch drop from ~1.96 to ~0.29 (1 conn) and ~2.09 to ~0.22
   (4 conn, io=4) when the same code stops deferring.

*Caveat on the within-branch comparison:* the `dont_read` runs also achieve larger average reads
(74–131 KB vs 56–66 KB), because the deferred path's back-pressure settles into lockstep with the
64 KB-writing client while `dont_read` lets the client run ahead and reads coalesce. Part of the
`dont_read` advantage is therefore fewer syscalls per byte, not purely the removed handoff. The
clean, confound-free claim is (1): **master ≈ branch whenever the branch runs callbacks inline.**

## Conclusion

The branch trades roughly **20–45% throughput and 50–90% higher p50 latency** on fast, non-blocking
callbacks for an **~8× gain when callbacks block**, plus the safety properties it documents: no
callback can occupy an io thread, per-session callback ordering, bounded queueing, and a buffer
handed to the callback with no copy.

Which side of that trade is right depends entirely on the workload:

- **Fast, in-process consumer** (parse and enqueue, count bytes, memcpy): master's inline path is
  materially better and the branch is a real regression.
- **Callbacks that do I/O, take contended locks, or otherwise block**: the branch is not merely safer
  but several times faster, and master's design actively falls over — its ceiling is
  `io_threads × (1 / callback_duration)` regardless of how many connections are waiting.

### Suggested follow-up

An **opt-in inline mode** would recover most of the loss without giving up the design. It cannot be
automatic: the hazard is whether the callback blocks, which no runtime check can determine — an idle
strand tells you nothing. It has to be a user-declared option (say
`options_t::inline_callbacks`) carrying master's contract: the callback must not block and must not
call the session's blocking APIs.

The mechanism already exists and is already reasoned about — the `defer` flag in
`make_session_callbacks` — including the subtlety that `OnSessionCreated` must be un-deferred
together with `OnSessionDataAvailable`, since deferring only one would let data be reported before
the session was. `OnDisconnected` should stay deferred either way; it is not lease-gated, so the
strand is still doing real ordering work there. The `dont_read` numbers above are a direct estimate
of what such a mode would deliver: parity with master.

## Reproducing

The harness is not checked in; it lives in the session scratchpad:

```
scratchpad/bench.cpp     # one source, compiles against both APIs
scratchpad/run.sh        # interleaved 5-rep driver
scratchpad/analyse.py    # medians + master-vs-branch deltas
scratchpad/results.jsonl # 200 raw rows
```

Build both libraries as Release from the two trees, then compile `bench.cpp` against each and run
`bench-<impl> <throughput|latency|churn|dontread|hol> <base_port> <seconds>`. Keep the base port
below 32768.
