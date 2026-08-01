# `tcp_*` — bugs and undefined/unexpected behaviour

Review of `sg::net::tcp_session`, `tcp_server`, `tcp_client`, `tcp_client_sync` and
`tcp_native` (plus the parts of `asio_io_pool` and `net.h` they depend on).

Findings were produced by code reading and then **verified with a purpose-built repro
harness**: each scenario runs in its own process under `timeout -s KILL`, so hangs
(`exit=137`), segfaults (`exit=139`) and `std::terminate` (`exit=134`) are unambiguous.
Everything marked *Confirmed* below has a reproduction; everything else is explicitly
labelled. Environment: clang, `cmake-build-container` (Debug), Linux 6.12, Boost.Asio as
vendored.

Legend: **Confirmed** = reproduced; **Latent** = real in code but no reachable trigger
found; **By inspection** = not reproduced (no repro attempted, or the sandbox can't show it).

---

## Summary

| # | Severity | Status | Area | Issue |
|---|---|---|---|---|
| [1](#1) | Critical | Confirmed | `tcp_session` | Throwing `onDisconnected` → `std::terminate`, or a session wedged in `stopping` forever |
| [2](#2) | Critical | Confirmed | `tcp_server` | Throwing user `OnDisconnected` leaks the session and makes the server unstoppable |
| [3](#3) | High | Confirmed | `tcp_server` | `future_get_once()` dereferences a null `m_context` |
| [4](#4) | High | Confirmed | `tcp_server` | A single transient `accept()` error permanently kills the listener |
| [5](#5) | High | Confirmed | `tcp_session` | `dont_read` busy-spins at EOF and never detects the disconnect |
| [6](#6) | High | Confirmed | `tcp_session` | Teardown and `set_keepalive`/`set_timeout` block forever if the io pool is not running |
| [7](#7) | Medium | Confirmed | `tcp_session` | `start()` on a stopped session is accepted → second `onDisconnected` |
| [8](#8) | Medium | Confirmed | `tcp_client_sync` | `m_read_leftover` survives disconnect → stale data on the next connection |
| [9](#9) | Medium | Confirmed | `tcp_client_sync` | Delimiter straddling a read boundary → spurious timeout |
| [10](#10) | Medium | Confirmed | `tcp_client_sync` | `read_some(n)` blocks until exactly `n` bytes arrive |
| [11](#11) | Medium | Confirmed | `tcp_session`/`tcp_client` | `timeout_msec == 0` is a zero deadline, not "disabled" |
| [12](#12) | Medium | Confirmed | `tcp_session` | A clean `stop_async()` is delayed by, and reported as, a write timeout |
| [13](#13) | Medium | Confirmed | `net.h` | `end_point()` leaves `.port` uninitialised |
| [14](#14) | Low | Confirmed | `tcp_session`/`tcp_server` | `options_t` is not an aggregate → designated initialisers don't compile |
| [15](#15) | Low | Confirmed | `tcp_client` | `disconnect()` error message names `connect()` |
| [16](#16) | Medium | By inspection | `tcp_client` | Not thread-safe against its own callbacks |
| [17](#17) | Medium | By inspection | `tcp_server` | Unsynchronised access to `m_acceptors` / `m_context` from the stopping thread |
| [18](#18) | Medium | By inspection | `tcp_session` | The io-thread guard only knows about *this* session's strand |
| [19](#19) | Low | By inspection | `tcp_server` | Destructor does not join the stopping thread before `m_pool` is destroyed |
| [20](#20) | Low | By inspection | various | Assorted API/semantic sharp edges |
| [21](#21) | Low | Latent | `tcp_client_sync` | `npos` arithmetic in `read_until()` can silently corrupt data |
| [22](#22) | Low | Latent | `tcp_client_sync` | `~tcp_client_sync()` calls a function that can throw |
| [23](#23) | Low | Latent | `tcp_session` | `writer()` records exceptions unconditionally / mis-maps `operation_aborted` |

---

## Confirmed

### <a name="1"></a>1. A throwing `onDisconnected` callback either terminates the process or wedges the session — Critical

`net/src/tcp_session.cpp:218-241`. `close_impl()` invokes the user callback at `:236`
and only *afterwards* stores the terminal state and wakes waiters (`:239-240`):

```cpp
if (m_callbacks.onDisconnected)
    m_callbacks.onDisconnected.invoke(*this, exPtr);   // <-- may throw

m_state.store(state_t::stopped, std::memory_order::release);
m_state.notify_all();
```

There are two outcomes, both bad, depending on who drives the close:

* **Close initiated off the io thread** (e.g. the application calls `stop_async()`):
  `close_impl` runs as a strand handler inside `io_context::run()`, so the exception
  escapes `run()` and then escapes the `asio_io_pool` worker thread's function →
  **`std::terminate`**.
* **Close initiated by the reader coroutine** (peer disconnected): `reader()` calls
  `close()` *outside* its `try` block (`:283`; likewise `writer()` at `:340`), so the
  exception escapes the coroutine and is **silently discarded** by
  `boost::asio::detached` (verified in `boost/asio/impl/detached.hpp:38-41` — the handler
  ignores the `exception_ptr`). The session is then stuck in `stopping` forever:
  `wait_until_stopped()` never returns, `is_connected()` stays `true`, and
  `~tcp_session`'s `assert` (`:34`) fires in debug builds.

Evidence:

```
=== disc_throws_local ===
  [INFO] calling stop_async() from main thread; onDisconnected will throw
  terminate called after throwing an instance of 'std::runtime_error'
    what():  boom
  exit=134 (SIGABRT/terminate)

=== disc_throws_peer ===
  state=2 (0=running 1=stop_requested 2=stopping 3=stopped) is_connected=1
  [CONFIRMED] session wedged in `stopping` after callback threw
  [CONFIRMED] wait_until_stopped() blocks forever
```

**Fix:** invoke the callback inside `try/catch`, and publish the terminal state from a
scope-exit guard so it happens on every path.

---

### <a name="2"></a>2. A throwing server `OnDisconnected` leaks the session and makes the server unstoppable — Critical

`net/src/tcp_server.cpp:296-315`. One lambda does three things in order: invoke the user
callback, erase the session, decrement the counter.

```cpp
m_pool.enqueue_detach([this, id, ex]() {
    if (m_callbacks.OnDisconnected)
        m_callbacks.OnDisconnected.invoke(*this, id, ex);   // <-- may throw

    { std::unique_lock lock(m_mutex); m_sessions.erase(id); }
    if (m_active_sessions.fetch_sub(1, std::memory_order::acq_rel) == 1)
        m_active_sessions.notify_all();
});
```

`dp::thread_pool::enqueue_detach` wraps the whole lambda in `try { … } catch (...) {}`
(`external/thread-pool/include/thread_pool/thread_pool.h:208-224`), so a throw in the
callback **skips the erase and the decrement**. `m_active_sessions` never reaches zero, so
the stopping thread's wait at `tcp_server.cpp:66-67` never returns → `stop_async()` never
completes → `~tcp_server` hangs on the `~jthread` join.

Evidence:

```
=== server_disc_throws ===
  clients_count() after peer disconnect = 1 (expected 0)
  [CONFIRMED] session leaked in m_sessions
  [CONFIRMED] server stop_async() never completes
```

**Fix:** do the bookkeeping first (or in a scope-exit guard) and wrap the user callback.
Note there is a commented-out test for callback-exception handling at
`test/src/tcp_server.cpp:286` — this is that hole.

---

### <a name="3"></a>3. `tcp_server::future_get_once()` dereferences a null `m_context` — High

`net/src/tcp_server.cpp:144-147`:

```cpp
void tcp_server::future_get_once() noexcept(false) {
    m_context->wait_for_stop();     // m_context is null before start()
    m_running.wait(true);
}
```

`m_context` is only created in `start()` (`:103`) and is reset again by `start()`'s failure
path (`:133`). Calling `future_get_once()` before `start()`, or after a `start()` that
threw, is a null-pointer dereference.

Evidence: `future_get_once` scenario → **SIGSEGV (exit=139)**.

**Fix:** early-return (or throw `std::logic_error`) when `!m_context`.

---

### <a name="4"></a>4. One transient `accept()` error permanently kills the listener — High

`net/src/tcp_server.cpp:244-251`. The listener's catch-all calls `stop_async()`, so a
per-connection, recoverable failure tears down every acceptor and every session:

```cpp
} catch (...) {
    //TODO: save and pass exception as exception_ptr to stopped callback
    stop_async();
}
```

`EMFILE`/`ENFILE` (fd exhaustion), `ECONNABORTED` and a `bad_alloc` from
`m_sessions.emplace` all land here. The reason is also discarded entirely — the
application just gets `OnStoppedListening` with no explanation.

Evidence (`RLIMIT_NOFILE` lowered, fd table exhausted, one connection attempted, then all
fds released):

```
=== emfile ===
  fd table exhausted (89 spare fds held)
  OnStoppedListening fired=1 is_stopped=1
  post-EMFILE connect threw: Connection refused [system:111]
  [CONFIRMED] one accept() failure permanently kills the listener
```

**Fix:** classify the error — retry the accept loop on transient per-connection errors,
and only stop on errors that are fatal to the acceptor (`operation_aborted`, `EBADF`).

---

### <a name="5"></a>5. `dont_read` busy-spins at EOF and never detects the disconnect — High

`net/src/tcp_session.cpp:256-261`:

```cpp
while (m_socket.is_open()) {
    if (m_options.dont_read) {
        co_await m_socket.async_wait(tcp::socket::wait_read, use_awaitable);
        if (m_callbacks.onDataAvailable)
            m_callbacks.onDataAvailable.invoke(*this, nullptr, 0);
    }
    ...
```

`wait_read` is level-triggered. Once the peer closes, the socket is readable *forever*, so
the loop never blocks: 100 % CPU, and the session is never torn down. The same happens if
the callback doesn't drain the socket, or if `onDataAvailable` is null.

Evidence:

```
=== dont_read ===
  CPU seconds burned in 1.0s wall after peer close: 0.997
  OnDisconnected fired: 0 times ; clients_count=1
  [CONFIRMED] busy-spin (>0.5 CPU-s per wall second)
  [CONFIRMED] disconnect never detected
```

`dont_read` has **no usages and no tests** anywhere in the repo — it is a wholly untested
code path.

**Fix:** after the wait, detect EOF (e.g. `recv(..., MSG_PEEK)` returning 0, or require the
callback to report how many bytes it consumed) and close the session.

---

### <a name="6"></a>6. Teardown and socket-option setters block forever if the io pool is not running — High

Two entry points, one root cause: work is handed to `m_strand` and then waited on
synchronously, with no timeout and no liveness check.

* `tcp_session.h:88-91` — `run_in_executor()` does `dispatch(m_strand, use_future(func))`
  then `fut.get()`. `set_keepalive()` and `set_timeout()` are built on it
  (`tcp_session.cpp:132-138`).
* `close()` (`tcp_session.cpp:212`) dispatches `close_impl` to the strand, and
  `wait_until_stopped()` (`:176-187`) waits for the state that only `close_impl` publishes.

If the pool has stopped (a shared `asio_io_pool` shut down by another owner, say), both
block forever.

Evidence:

```
=== run_in_executor ===
  [INFO] pool stopped; calling session().set_timeout(1000)
  [CONFIRMED] set_timeout() blocks forever on a stopped pool

=== write_dead_pool ===
  [CONFIRMED] stop_async()+wait_until_stopped() hangs after write()
=== write_dead_pool_control (no pending write) ===
  control: completed=0        <-- hangs either way
```

**Fix:** fail fast (throw) when the executor's context is not running, and/or give
`wait_until_stopped()` a timeout overload.

A related side effect: messages passed to `write()` while the pool is down are accepted
without error and silently never sent.

---

### <a name="7"></a>7. `start()` on a stopped session is accepted, and fires `onDisconnected` twice — Medium

`net/src/tcp_session.cpp:38-40`. The CAS only requires `stopped`, which is also the
*terminal* state, so a dead session can be restarted. It then applies socket options to a
closed fd, throws, and the failure path fires `onDisconnected` a second time. `m_exception`,
`m_write_scheduled` and any queued `m_write_msgs` are never reset either.

Evidence:

```
=== restart_session ===
  after clean disconnect: onDisconnected count = 1, state=3
  start() threw: Bad file descriptor
  onDisconnected count now = 2
  [CONFIRMED] restart of a dead session fires onDisconnected twice
```

**Fix:** add a distinct `never_started` state, or refuse `start()` once the session has
been stopped.

---

### <a name="8"></a>8. `tcp_client_sync`: buffered data survives disconnect/reconnect — Medium

`net/src/tcp_client_sync.cpp:78-108`. Neither `connect()` nor `disconnect()` clears
`m_read_leftover`, so bytes left over from a previous socket are returned as the first
bytes read from the next one.

Evidence (server sends `a\r\nSTALE\r\n`, client reads one line, reconnects to a server
sending `FRESH\r\n`):

```
=== sync_leftover ===
  first connection returned  "a"
  second connection returned "STALE" (expected FRESH)
  [CONFIRMED] stale buffered data leaks into the next connection
```

**Fix:** `m_read_leftover.clear()` in both `connect()` and `disconnect()`.

---

### <a name="9"></a>9. `tcp_client_sync::read_until()`: a delimiter straddling a read boundary is missed — Medium

`net/src/tcp_client_sync.cpp:27-40`. `async_read_until` is given a *fresh* buffer, not
`m_read_leftover + new data`, so it only searches the newly-read bytes. If the leftover
ends with `"\r"` and the next byte is `"\n"`, the delimiter is already complete but
`async_read_until` cannot see it and keeps reading — until the *next* delimiter arrives, or
the timeout expires.

Evidence (leftover `"b\r"` from a previous call, server then sends exactly `"\n"`):

```
=== sync_client ===
  first read_until -> "a\r\n" (expect a\r\n)
  second read_until threw: operation timed out
  [CONFIRMED] delimiter straddling a read boundary is not seen -> spurious timeout
```

**Fix:** seed the dynamic buffer with `m_read_leftover` (or check
`leftover + first-new-byte` before issuing the read).

---

### <a name="10"></a>10. `read_some(n)` blocks until exactly `n` bytes arrive — Medium

`net/src/tcp_client_sync.cpp:51-76`. `async_read` with a capped dynamic buffer uses
`transfer_all`, so it waits for the full `n`. The name promises `read_some` semantics
("return what's available"); it is really `read_exactly`.

Evidence:

```
=== read_some ===
  read_some(10) with 3 bytes available threw: operation timed out
  [CONFIRMED] read_some(n) blocks until exactly n bytes arrive
```

Related: on a short read, `m_read_leftover.substr(size)` at `:72` throws
`std::out_of_range`.

**Fix:** rename to `read_exactly()`, or use `transfer_at_least(1)` and return what arrived.

---

### <a name="11"></a>11. `timeout_msec == 0` means "zero deadline", not "no timeout" — Medium

`tcp_session.cpp:316-319` and `tcp_client.cpp:71-74` pass `timeout_msec` straight to
`boost::asio::cancel_after`. `0` is therefore a deadline of *now*: any operation that
cannot finish in a single event-loop turn is cancelled and reported as
`exceptions::net::time_out`.

Evidence (64 MiB write to a peer that never reads):

```
  timeout_msec=0     -> write failed after     53 ms, error="operation timed out"
  timeout_msec=1500  -> write failed after   1560 ms, error="operation timed out"
```

(A loopback `connect()` with `timeout_msec = 0` *does* usually succeed, because it
completes before the timer is serviced — which makes the behaviour non-deterministic rather
than safe.)

**Fix:** treat `0` as "no timeout" (skip `cancel_after`), or reject it in `options_t`.

---

### <a name="12"></a>12. A clean `stop_async()` is delayed by, and reported as, a write timeout — Medium

When a write is in flight, `stop_async()` sees `m_write_scheduled == true` and delegates the
close to `writer()` (`tcp_session.cpp:150-174`). The writer must first finish or time out
the pending `async_write`, so a user-requested clean shutdown blocks for up to
`timeout_msec` and then surfaces as an error rather than a clean close.

Evidence (1000 ms timeout, pending write to a deaf peer):

```
=== pending_write_stop ===
  stop_async() -> session closed after 910 ms; onDisconnected exception="operation timed out"
```

**Fix:** on `stop_requested`, cancel the in-flight write immediately instead of waiting out
its deadline, and report the shutdown as clean.

---

### <a name="13"></a>13. `end_point()` leaves `.port` uninitialised — Medium (UB)

`net/include/sg/net/net.h:55-61`:

```cpp
struct end_point{
    end_point() = default;          // default-init => .port is indeterminate
    end_point(std::string ip, port_t port): ip(std::move(ip)), port(port) {}

    std::string ip;
    port_t port;                    // no initialiser
};
```

`end_point e;` leaves `port` indeterminate; reading it is UB. `tcp_session` only escapes
this because its members use `{}` value-initialisation.

Evidence: after dirtying the stack, `default-constructed end_point.port = 43947`.

**Fix:** `port_t port{0};`.

---

### <a name="14"></a>14. `options_t` is not an aggregate — designated initialisers don't compile — Low

The clang-36032 workaround (`tcp_session.h:25`, `tcp_server.h:51`) adds a user-provided
default constructor, which makes the struct a non-aggregate. So the natural call site

```cpp
client.connect(ep, onData, onDisc, {.timeout_msec = 500});
```

fails with *"initialization of non-aggregate type 'tcp_session::options_t' with a
designated initializer list"*. Every caller must construct a named object and assign
fields. The referenced clang bug is long fixed; the workaround is worth re-testing against
the currently supported compilers.

---

### <a name="15"></a>15. `tcp_client::disconnect()` reports the wrong function name — Low

`net/src/tcp_client.cpp:107-110` — copy-paste from `connect()`:

```
disconnect() from callback threw: "tcp_client::connect() must not be called from the
I/O thread (e.g. from a callback)"
```

---

## By inspection (not reproduced)

### <a name="16"></a>16. `tcp_client` is not thread-safe against its own callbacks — Medium

`m_session` is a plain `shared_ptr` read by `is_connected()`, `session()` and
`disconnect()` and written by `connect()` (`tcp_client.cpp:87`, `:97`) with no
synchronisation, while callbacks run on the io thread. `session()` also returns a raw
`tcp_session&` that dangles as soon as a later `connect()` reassigns `m_session`.

### <a name="17"></a>17. `tcp_server`: unsynchronised access from the stopping thread — Medium

`stop_async()`'s thread reads the `m_context` `shared_ptr` (`tcp_server.cpp:33`, `:55`,
`:69`) and iterates `m_acceptors` (`:34`) with no lock, while `start()` writes both
(`:100`, `:103`, `:106`) and its failure path clears them (`:132-133`).
`start()`'s `m_stop_in_operation.store(false)` (`:94`) can also clobber a concurrent stop
request. Confirming this properly needs a TSAN build of `sg_net` + `sg_common`.

### <a name="18"></a>18. The io-thread guard only knows about *this* session's strand — Medium

`running_in_io_thread()` (`tcp_session.cpp:146`) checks
`m_strand.running_in_this_thread()`. Calling `wait_until_stopped()` — or
`tcp_client::disconnect()` — from *another* session's callback on the same pool passes the
guard and blocks a pool worker. It survives today only because
`strand_executor_service::dispatch` falls through to `io_context::executor::dispatch`,
which runs the handler inline when the calling thread is inside `run()` and the target
strand is uncontended. If that strand is concurrently locked and no worker is free, it
deadlocks. The same reasoning applies to `start()`'s failure path
(`tcp_session.cpp:74-75`), which calls `wait_until_stopped()` from
`tcp_server::listener()` — i.e. from an io thread — with `no_threads` defaulting to **1**.

### <a name="19"></a>19. `~tcp_server` does not join the stopping thread before `m_pool` dies — Low

`tcp_server.cpp:14-21` only stops and waits when `m_context && m_context->is_running()`,
and never joins `m_stopping_thread` explicitly. Because `m_stopping_thread` is declared
*before* `m_pool` (`tcp_server.h:100-102`), **`m_pool` is destroyed first** and the join
happens afterwards; the stopping thread's final `m_running.store(false)` can also race with
the destruction of `m_running`. 300 start/`stop_async`/destroy race cycles did **not**
crash, so treat this as hardening rather than a live defect — but joining the thread at the
top of the destructor body is the correct shape.

### <a name="20"></a>20. Assorted sharp edges — Low

* `tcp_server::write()/session()/disconnect()` throw an undocumented `std::out_of_range`
  (`m_sessions.at`), and the id is inherently racy: one valid at the start of
  `OnSessionDataAvailable` can be gone by the time it is used.
* `clients_count()`/`sessions()` lag reality — removal happens asynchronously on `m_pool`
  *after* the user callback returns (`tcp_server.cpp:304-311`).
* A connection accepted while a stop is in progress (`tcp_server.cpp:224`) is dropped with
  no `shutdown()` and **no callbacks at all**. (My repro could not distinguish this — a
  silently dropped session fires neither callback, so the counters stay balanced.)
* No way to discover the bound port when listening on port 0; acceptor endpoints are not
  exposed.
* `tcp_session::is_connected()` (`:189`) really means "not fully stopped" — it returns
  `true` throughout `stop_requested` and `stopping`. `tcp_client_sync::is_connected()`
  (`:21`) is just `socket.is_open()`, which stays true after the peer disconnects.
* `write()` before `start()` throws *"attempt to write to tcp_session after a disconnection
  was requested"* (`:87`) — wrong message for a never-started session.
* Exceptions thrown by the user's `onDataAvailable` are caught by `reader()` and delivered
  to `onDisconnected` as if they were network errors.
* `reader()` sizes its buffer from `SO_RCVBUF` (`:251-255`), which Linux reports doubled —
  ~256 KB per session by default (~38 MB for the 150-client test), and `recv_buffer_size`
  silently doubles as the read-buffer size at 2× the requested value.
* DNS resolution in `tcp_client::connect()` (`:54-55`) is synchronous and **not** covered by
  `options.timeout_msec` (measured: 123 ms elapsed against a 50 ms budget on an instant
  NXDOMAIN; a black-holed resolver would be seconds).
* `tcp_native::set_keepalive()` with `enable == false` leaves idle/interval/count untouched,
  so a later re-enable silently reuses stale values; on Apple only `TCP_KEEPALIVE` is
  applied and `interval_seconds`/`count` are discarded without warning
  (`tcp_native.cpp:51-63`). Typo: `enbaled` (`tcp_native.h:24`, `.cpp:81`).
* On Linux `get_recv_buffer_size()` will not match what `set_recv_buffer_size()` was given
  (the kernel doubles it).

---

## Latent (real in code, no reachable trigger found)

### <a name="21"></a>21. `npos` arithmetic in `read_until()` can silently corrupt data

`net/src/tcp_client_sync.cpp:43-47`:

```cpp
// .find() will always find deliemeter as .async_read_until() was used
auto pos = m_read_leftover.find(delimiter) + delimiter.length();
```

If the read completed with `operation_aborted` — which `throw_error_if_not_timedout`
(`:9-12`) deliberately treats as success — no delimiter is present, `find()` returns
`npos`, and `npos + delimiter.length()` **wraps** to `length()-1`. `substr` then returns a
garbage prefix and leaves the buffer misaligned, with no error raised.

Not currently reachable single-threaded: the only `close()` is inside `run()`, which then
throws, so `read_until()` never proceeds past it. Still worth fixing defensively — the
comment's premise is only true on the success path.

### <a name="22"></a>22. `~tcp_client_sync()` calls a function that can throw

`net/src/tcp_client_sync.cpp:19` — `~tcp_client_sync() { disconnect(); }`, and
`disconnect()` → `run()` can throw `exceptions::net::time_out` (`:159`), rethrow a handler
exception (`:147`), or throw from `m_socket.close()` (`:104`). Destructors are implicitly
`noexcept`, so that is `std::terminate`.

Not reproduced: after a timeout, `run()` has already closed the socket, so `disconnect()`
is a no-op (verified: `is_connected() == 0`, destructor completed cleanly). Wrap the body in
`try/catch` anyway.

### <a name="23"></a>23. `writer()` records exceptions unconditionally and mis-maps `operation_aborted`

`net/src/tcp_session.cpp:323-338`. Two inconsistencies with `reader()`/`start()`:

* `operation_aborted` is mapped to `exceptions::net::time_out` (`:324-326`), but it is also
  what a normal `close_impl()` produces under an in-flight write.
* The catch block stores the exception with no `if (m_state == running)` guard, unlike
  `reader()` (`:275`) and `start()` (`:67`).

Every timeout I could actually surface was a genuine one, and when a close *does* abort a
write, `close_impl()` has already delivered the (null) exception, so the mis-mapped one is
discarded and never reaches the application. Code-consistency issue rather than an
observable bug.

---

## Suggested fix order

1. **[#1](#1)** and **[#2](#2)** — a throwing user callback either kills the process or
   permanently wedges the session and the server. Note the commented-out test at
   `test/src/tcp_server.cpp:286` sits exactly on this hole.
2. **[#3](#3)** — one-line null check.
3. **[#4](#4)** — classify accept errors instead of stopping on all of them.
4. **[#5](#5)** — `dont_read` is unusable as written (and untested).
5. **[#6](#6)** — fail fast instead of blocking forever on a dead pool.
6. **[#8](#8)**, **[#9](#9)** — `tcp_client_sync` returns wrong data / times out spuriously.

## Reproducing

The harness is two standalone programs, one scenario per process:

```sh
make -C cmake-build-container sg_net -j"$(nproc)"

clang++ -DBOOST_ASIO_DISABLE_ERROR_LOCATION -DBOOST_ASIO_DISABLE_SOURCE_LOCATION -DFMT_SHARED \
  -I net/include -I cmake-build-container/include -I external/thread-pool/include \
  -I common/include -I cmake-build-container/_deps/pfs-src/include \
  -g -std=gnu++20 -Wall -Wextra -o verify verify.cpp \
  -L cmake-build-container/net -L cmake-build-container/common -lsg_net -lsg -lfmt -lpthread \
  -Wl,-rpath,"$PWD/cmake-build-container/net" -Wl,-rpath,"$PWD/cmake-build-container/common"

timeout -s KILL 25 ./verify <scenario> <port>   # exit 137 = hung, 139 = segv, 134 = terminate
```

Scenarios: `future_get_once`, `disc_throws_local`, `disc_throws_peer`,
`server_disc_throws`, `dont_read`, `run_in_executor`, `write_dead_pool`,
`write_dead_pool_control`, `emfile`, `restart_session`, `zero_timeout`, `wrong_message`,
`sync_client`, `sync_leftover`, `read_some`, `endpoint`; and in the second program
`write_timeout <port> <msec>`, `pending_write_stop`, `dns`, `sync_dtor`, `dtor_stress`,
`accept_during_stop`.

These are worth folding into `test/src/` as Catch2 regression cases once the fixes land.
