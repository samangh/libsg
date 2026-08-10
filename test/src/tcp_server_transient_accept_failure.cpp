/* Regression test for https://github.com/samangh/libsg/issues/9: a transient, per-connection
 * accept() failure used to tear down every acceptor and every session.
 *
 * POSIX only: it needs RLIMIT_NOFILE to provoke an EMFILE out of accept(2). */

#ifndef _WIN32
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "helpers.h"

#include "sg/net/tcp_client.h"
#include "sg/net/tcp_client_sync.h"
#include "sg/net/tcp_server.h"

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>

#include <memory>
#include <random>
#include <stdexcept>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>

using namespace sg::net;
static port_t PORT = 4444; // 55555 can't be used on macOS!

namespace {

/* Holds every spare file descriptor in the process, so that the next accept(2) fails with
 * EMFILE. Both the descriptors and the original RLIMIT_NOFILE are given back on destruction.
 *
 * The soft limit is first lowered to just above what the process is already using, so that
 * exhausting the table stays cheap on hosts that allow a great many open files -- and so that we
 * never cut the limit below what the rest of the test binary already holds open. */
class fd_exhauster {
  public:
    fd_exhauster() {
        if (::getrlimit(RLIMIT_NOFILE, &m_original) != 0)
            throw std::runtime_error("getrlimit(RLIMIT_NOFILE) failed");

        /* descriptors are handed out lowest-free-first, so a probe tells us roughly how many
         * are in use */
        const int probe = ::dup(STDIN_FILENO);
        if (probe < 0)
            throw std::runtime_error("no spare file descriptor to probe with");
        ::close(probe);

        rlimit reduced = m_original;
        reduced.rlim_cur =
            std::min<rlim_t>(m_original.rlim_cur, static_cast<rlim_t>(probe) + spare_count);
        if (::setrlimit(RLIMIT_NOFILE, &reduced) != 0)
            throw std::runtime_error("setrlimit(RLIMIT_NOFILE) failed");

        for (int fd = ::dup(STDIN_FILENO); fd >= 0; fd = ::dup(STDIN_FILENO))
            m_fds.push_back(fd);
    }

    ~fd_exhauster() {
        release(m_fds.size());
        ::setrlimit(RLIMIT_NOFILE, &m_original);
    }

    fd_exhauster(const fd_exhauster&)            = delete;
    fd_exhauster& operator=(const fd_exhauster&) = delete;

    [[nodiscard]] size_t held() const { return m_fds.size(); }

    /* Re-takes every descriptor that is free again, including any released by other threads */
    void refill() {
        for (int fd = ::dup(STDIN_FILENO); fd >= 0; fd = ::dup(STDIN_FILENO))
            m_fds.push_back(fd);
    }

    /* Hands back `count` descriptors, so that the caller has room to open a socket of its own */
    void release(size_t count) {
        for (size_t i = 0; i < count && !m_fds.empty(); ++i) {
            ::close(m_fds.back());
            m_fds.pop_back();
        }
    }

  private:
    static constexpr rlim_t spare_count = 32;

    rlimit m_original{};
    std::vector<int> m_fds;
};

/* A bare POSIX connect, in two halves so that the caller can take its descriptor before the
 * table is sealed and only connect afterwards. The library's own clients would need several
 * descriptors of their own (epoll, eventfd, ...), which is precisely what is scarce here. */
bool raw_connect(int fd, sg::net::port_t port) {
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");

    return ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

int raw_connect(sg::net::port_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    if (!raw_connect(fd, port)) {
        ::close(fd);
        return -1;
    }

    return fd;
}

} // namespace

TEST_CASE("tcp_server: a transient accept() failure does not kill the listener",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog("DEADLOCK: EMFILE regression test stalled");

    std::atomic_int sessions{0};
    std::atomic_int stop_count{0};
    std::atomic_int accept_errors{0};
    std::atomic_bool saw_emfile{false};
    std::binary_semaphore start_sem{0};

    tcp_server::started_listening_cb_t onStart = [&](tcp_server&) { start_sem.release(); };
    tcp_server::stopped_listening_cb_t onStop  = [&](tcp_server&, std::exception_ptr) {
        stop_count++;
    };
    tcp_server::session_created_cb_t onSession = [&](tcp_server&, tcp_server::session_id_t) {
        sessions++;
    };
    /* Runs on an io thread, so it records what it saw and leaves the checking to the test
     * thread, where Catch2's macros are safe to use.
     *
     * It then throws, every single time: an error handler that throws must not be able to take
     * down the listener it is reporting on. The assertions at the end of this test -- the
     * connection is accepted, the server is still listening -- are what prove that it doesn't. */
    tcp_server::accept_error_cb_t onAcceptError = [&](tcp_server&, std::exception_ptr ex) {
        accept_errors++;
        try {
            std::rethrow_exception(ex);
        } catch (const boost::system::system_error& err) {
            if (err.code() == boost::asio::error::no_descriptors)
                saw_emfile.store(true);
        } catch (...) {
        }

        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_server::CallBacks cb;
    cb.OnStartedListening = onStart;
    cb.OnStoppedListening = onStop;
    cb.OnSessionCreated   = onSession;
    cb.OnAcceptError      = onAcceptError;

    end_point ep("127.0.0.1", PORT);

    tcp_server l;
    l.start({ep}, cb);
    start_sem.acquire();

    int client_fd = -1;
    {
        fd_exhauster fds;
        REQUIRE(fds.held() > 4);

        /* A few descriptors back, so that the client can take one for itself -- with a margin,
         * because another thread may well grab one of them first. */
        fds.release(4);
        client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(client_fd >= 0);

        /* Seal the table again before connecting, so that the accept(2) this provokes has no
         * descriptor to succeed with. The kernel completes the handshake from the listen backlog
         * either way, so connect() succeeds and the accept() fails with EMFILE; what becomes of
         * that connection afterwards is kernel-specific, and dealt with further down. */
        fds.refill();
        REQUIRE(raw_connect(client_fd, PORT));

        /* Whenever the whole binary is run in one process, background threads left over from
         * earlier test cases occasionally close a descriptor. The server would then accept with
         * it, hiding the very failure this test is about, so a thread re-takes anything that
         * becomes free for as long as the window is open. */
        std::atomic_bool hammer_stop{false};
        std::vector<int> stolen;
        std::jthread hammer([&]() {
            while (!hammer_stop.load(std::memory_order_relaxed)) {
                const int fd = ::dup(STDIN_FILENO);
                if (fd >= 0)
                    stolen.push_back(fd);
                else
                    std::this_thread::yield();
            }
        });

        /* A healthy accept(2) lands within microseconds of the connect, so a session that still
         * does not exist proves the listener really did hit EMFILE. Sampled over a window kept
         * deliberately short, because the whole scheme rests on nobody else freeing a descriptor
         * in the meantime. */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const int sessions_under_pressure = sessions.load();

        /* Then hold the pressure on for a good while longer, so that the listener has to fail,
         * back off and retry -- many times over on a kernel that leaves the failed connection
         * queued, at least once everywhere. Nothing is asserted about this window: by now a
         * descriptor freed elsewhere in the process may well have let the connection through,
         * and that is harmless. */
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        hammer_stop.store(true, std::memory_order_relaxed);
        hammer.join();
        for (const int fd : stolen)
            ::close(fd);

        if (sessions_under_pressure != 0) {
            /* Another thread freed a descriptor before the sample was taken, so accept(2) never
             * failed and there is nothing here to regress against. Reported rather than passed
             * off as a success, so that a test that has quietly stopped exercising EMFILE cannot
             * masquerade as a green one. */
            ::close(client_fd);
            SKIP("could not keep the descriptor table exhausted; the EMFILE path was not "
                 "exercised on this run");
        }
    } // descriptors and RLIMIT_NOFILE are restored here

    /* Whether the connection queued during the blackout survives it is up to the kernel, so
     * nothing is asserted about it:
     *
     *   - Linux reserves the descriptor before it dequeues the connection
     *     (__sys_accept4_file()), so accept(2) fails with the connection still queued and the
     *     listener collects it on one of its retries.
     *   - Darwin dequeues first and, finding it cannot allocate a descriptor, drops the connection
     *     -- xnu, uipc_syscalls.c: "Don't put this back on the socket like we used to, that just
     *     causes the client to spin. Drop the socket." The client is reset and there is nothing
     *     left for the listener to accept.
     *
     * What this test is about is the listener, and the fresh connection below proves that far
     * more directly than the queued one ever did.
     *
     * The sleep lets whichever of the two is going to happen happen, so that the sample taken
     * below is not taken in the middle of it: the backoff has reached its ceiling by now, so the
     * first retry after the pressure lifts can be a good fraction of a second away. */
    std::this_thread::sleep_for(std::chrono::seconds(1));

    REQUIRE_FALSE(l.is_stopped());
    REQUIRE(stop_count == 0);
    REQUIRE(l.last_error() == nullptr);

    /* the failures were recoverable, so they are reported through OnAcceptError() and not as a
     * reason for stopping */
    REQUIRE(accept_errors.load() > 0);
    REQUIRE(saw_emfile.load());

    /* And the listener is still good for new connections. Before the fix it was already gone by
     * this point, so nothing more was ever accepted. */
    const int sessions_before = sessions.load();

    const int second_fd = raw_connect(PORT);
    REQUIRE(second_fd >= 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (sessions.load() <= sessions_before && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(sessions.load() == sessions_before + 1);

    ::close(client_fd);
    ::close(second_fd);

    l.stop_async();
    l.future_get_once();
    REQUIRE(stop_count == 1);
}

#endif

