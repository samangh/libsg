#include "sg/debug.h"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_contains.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include <memory>
#include <sstream>
#include <stdexcept>

namespace {

void test_throw(int a) {
    std::ignore = a;
    SG_THROW(std::runtime_error, "msg");
}

/* Traces into a buffer for as long as it is in scope. */
struct scoped_trace {
    std::shared_ptr<std::ostringstream> buffer{std::make_shared<std::ostringstream>()};
    sg::debug::exception_trace_mode previous_mode{sg::debug::trace_mode()};
    std::shared_ptr<std::ostream> previous_stream{sg::debug::trace_stream()};

    explicit scoped_trace(sg::debug::exception_trace_mode mode) {
        sg::debug::set_trace_stream(buffer);
        sg::debug::set_trace_mode(mode);
    }

    ~scoped_trace() {
        sg::debug::set_trace_mode(previous_mode);
        sg::debug::set_trace_stream(previous_stream);
    }

    [[nodiscard]] std::string text() const { return buffer->str(); }
};

} // namespace

TEST_CASE("sg::debug: check SG_THROW(..) leaves the exception alone", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::details);

    REQUIRE_THROWS_AS(test_throw(2), std::runtime_error);
    REQUIRE_THROWS_WITH(test_throw(2), Catch::Matchers::Equals("msg"));
}

TEST_CASE("sg::debug: check exception_trace_mode::none traces nothing", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::none);

    REQUIRE_THROWS(test_throw(2));
    REQUIRE(trace.text().empty());
}

TEST_CASE("sg::debug: check exception_trace_mode::details traces the location", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::details);

    REQUIRE_THROWS(test_throw(2));

    REQUIRE_THAT(trace.text(), Catch::Matchers::ContainsSubstring("test_throw"));
    REQUIRE_THAT(trace.text(), Catch::Matchers::ContainsSubstring("debug.cpp("));
}

TEST_CASE("sg::debug: check a null stream traces nothing", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::details);
    sg::debug::set_trace_stream(nullptr);

    REQUIRE_THROWS(test_throw(2));
    REQUIRE(trace.text().empty());
}

TEST_CASE("sg::debug: check set_trace_mode(..) and trace_mode()", "[sg::debug]") {
    const auto original = sg::debug::trace_mode();

    sg::debug::set_trace_mode(sg::debug::exception_trace_mode::details);
    REQUIRE(sg::debug::trace_mode() == sg::debug::exception_trace_mode::details);

    sg::debug::set_trace_mode(original);
    REQUIRE(sg::debug::trace_mode() == original);
}

TEST_CASE("sg::debug: check the stream is kept alive by us", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::details);

    std::weak_ptr<std::ostringstream> weak;
    {
        auto buffer = std::make_shared<std::ostringstream>();
        weak = buffer;
        sg::debug::set_trace_stream(buffer);
    }

    /* we are the only owner left */
    REQUIRE_FALSE(weak.expired());

    REQUIRE_THROWS(test_throw(2));
    REQUIRE_THAT(weak.lock()->str(), Catch::Matchers::ContainsSubstring("test_throw"));
}

TEST_CASE("sg::debug: check exception_trace_mode::stacktrace", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::stacktrace);

#ifdef LIBSG_STACKTRACE
    REQUIRE(sg::debug::trace_mode() == sg::debug::exception_trace_mode::stacktrace);

    REQUIRE_THROWS(test_throw(2));
    REQUIRE_THAT(trace.text(), Catch::Matchers::ContainsSubstring("stacktrace"));
#else
    REQUIRE(sg::debug::trace_mode() == sg::debug::exception_trace_mode::details);

    REQUIRE_THROWS(test_throw(2));
    REQUIRE_THAT(trace.text(), Catch::Matchers::ContainsSubstring("test_throw"));
#endif
}

TEST_CASE("sg::debug: check write_trace(..) writes a line", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::details);

    sg::debug::write_trace("first");
    sg::debug::write_trace("second");

    REQUIRE(trace.text() == "first\nsecond\n");
}

TEST_CASE("sg::debug: check write_trace(..) ignores the trace mode", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::none);

    sg::debug::write_trace("written anyway");

    REQUIRE_THAT(trace.text(), Catch::Matchers::ContainsSubstring("written anyway"));
}

TEST_CASE("sg::debug: check write_trace(..) with a null stream discards", "[sg::debug]") {
    scoped_trace trace(sg::debug::exception_trace_mode::details);
    sg::debug::set_trace_stream(nullptr);

    sg::debug::write_trace("dropped");

    REQUIRE(trace.text().empty());
}
