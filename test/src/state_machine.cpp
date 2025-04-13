#include <sg/state_machine.h>

#include <catch2/catch_test_macros.hpp>
#include <semaphore>
#include <string>

TEST_CASE("sg::state_machine: check state transition and state tick callbacks",
          "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
        add_to_counter2
    };
    typedef sg::state_machine<state> state_machine;

    std::atomic<int> counter1 {0};
    std::atomic<int> counter2 {0};
    state_machine sm;

    sg::state_machine<state>::state_tick_callback_t counter1_tick = [&counter1](state_machine& sm_, state_machine::current_state_details) {
        counter1++;
        sm_.set_state(state::add_to_counter2);
    };

    sg::state_machine<state>::state_tick_callback_t counter2_tick = [&counter2](sg::state_machine<state>& sm_, state_machine::current_state_details) {
        counter2++;
        sm_.request_stop();
    };

    sm.add_state(state::add_to_counter1);
    sm.add_state(state::add_to_counter2);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);
    sm.add_tick_callback(state::add_to_counter2, counter2_tick);
    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE(counter1==1);
    REQUIRE(counter2==1);
    REQUIRE_NOTHROW(sm.future().get());

}

TEST_CASE("sg::state_machine: check state entry/exit callbacks",
          "[sg::state_machine]") {

    enum class state {
        add_to_counter1,
        stop_state
    };
    typedef sg::state_machine<state> state_machine;

    std::atomic<int> counter1 {0};
    std::atomic<int> counter2 {0};
    sg::state_machine<state> sm;

    sg::state_machine<state>::state_change_callback_t counter1_start_cb = [&counter1](state_machine&, state_machine::state_change_details det) {
        counter1++;
        if (det.old_state!=state::add_to_counter1 || det.new_state != det.old_state)
            throw std::logic_error("bad implementation");
    };

    sg::state_machine<state>::state_tick_callback_t counter1_tick = [&](state_machine& sm_, state_machine::current_state_details det) {
        sm_.set_state(state::stop_state);
        if (det.current_state!=state::add_to_counter1)
            throw std::logic_error("bad implementation");
    };


    sg::state_machine<state>::state_change_callback_t counter1_stop1_cb = [&counter1](state_machine&, state_machine::state_change_details det) {
        counter1 = counter1+2;
        if (det.old_state!=state::add_to_counter1)
            throw std::logic_error("bad implementation");
        if (det.new_state!=state::stop_state)
            throw std::logic_error("bad implementation");

    };

    sg::state_machine<state>::state_change_callback_t counter1_stop2_cb = [&counter1](state_machine&, state_machine::state_change_details det) {
        counter1 = counter1+3;
        if (det.old_state!=state::add_to_counter1)
            throw std::logic_error("bad implementation");
        if (det.new_state!=state::stop_state)
            throw std::logic_error("bad implementation");
    };

    sg::state_machine<state>::state_tick_callback_t stop_state_tick = [](sg::state_machine<state>& sm_, state_machine::current_state_details det) {
        sm_.request_stop();
        if (det.current_state!=state::stop_state)
            throw std::logic_error("bad implementation");
    };

    sg::state_machine<state>::state_change_callback_t cstop_state_start_cb = [&counter2](state_machine&, state_machine::state_change_details det) {
       counter2++;
       if (det.old_state!=state::add_to_counter1 || det.new_state != state::stop_state)
           throw std::logic_error("bad implementation");
    };

    sg::state_machine<state>::state_change_callback_t stop_state_stop_cb = [&counter2](state_machine&, state_machine::state_change_details det) {
        counter2 = counter2 + 6;
        if (det.old_state!=state::stop_state || det.new_state != det.old_state)
            throw std::logic_error("bad implementation");
    };


    sm.add_state(state::add_to_counter1);
    sm.add_entry_callback(state::add_to_counter1, counter1_start_cb);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);
    sm.add_exit_callback(state::add_to_counter1, counter1_stop1_cb);
    sm.add_exit_callback(state::add_to_counter1, counter1_stop2_cb);

    /* state2 will stop */
    sm.add_state(state::stop_state);
    sm.add_entry_callback(state::stop_state, cstop_state_start_cb);
    sm.add_tick_callback(state::stop_state, stop_state_tick);
    sm.add_exit_callback(state::stop_state, stop_state_stop_cb);

    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE(counter1==6);
    REQUIRE(counter2==7);
    REQUIRE_NOTHROW(sm.future().get());
}

TEST_CASE("sg::state_machine: top state machine started / stopped callback",
          "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };
    typedef sg::state_machine<state> state_machine;

    std::atomic<bool> counter_state_entered {false};
    std::atomic<bool> counter_state_exited {false};
    std::atomic<bool> counter_sm_started {false};
    std::atomic<bool> counter_sm_stopped {false};
    std::atomic<int> counter {0};
    state_machine sm;

    /* use same callback for entry*/
    state_machine::state_change_callback_t counter_entry_cb= [&](state_machine& , state_machine::state_change_details) {
        // SM start callback must be called
        if (!counter_sm_started)
            throw std::logic_error("something is wrong");
        counter_state_entered = true;

    };

    /* tick callback */
    state_machine::state_tick_callback_t counter_tick = [&](state_machine& sm_, state_machine::current_state_details) {
        // The state entry callback must have been called
        if (!counter_state_entered)
            throw std::logic_error("something is wrong");
        counter++;
        sm_.request_stop();
    };

    /* use same callback for exit*/
    state_machine::state_change_callback_t counter_exit_cb = [&](state_machine& , state_machine::state_change_details) {
        // SM start callback must be called
        if (counter!=1)
            throw std::logic_error("something is wrong");
        if (counter_sm_stopped)
            throw std::logic_error("something is wrong");
        counter_state_exited = true;

    };

    state_machine::started_stopped_callback_t sm_started_cb = [&](state_machine&){
        if (counter_state_entered || counter_state_exited || counter!=0)
            throw std::logic_error("something is wrong");
        counter_sm_started = true;
    };

    state_machine::started_stopped_callback_t sm_stopped_cb = [&](state_machine&){
        if (!counter_state_entered || !counter_state_exited || counter!=1)
            throw std::logic_error("something is wrong");
        counter_sm_stopped = true;
    };


    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter_tick);
    sm.add_entry_callback(state::add_to_counter1, counter_entry_cb);
    sm.add_exit_callback(state::add_to_counter1, counter_exit_cb);

    sm.set_machine_started_cb(sm_started_cb);
    sm.set_machine_stopped_cb(sm_stopped_cb);

    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE(counter==1);
    REQUIRE(counter_state_entered.load());
    REQUIRE(counter_state_exited.load());
    REQUIRE(counter_sm_started.load());
    REQUIRE(counter_sm_stopped.load());
    REQUIRE_NOTHROW(sm.future().get());
}

TEST_CASE("sg::state_machine: check expections are caught in tick callbacks", "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };
    typedef sg::state_machine<state> state_machine;

    state_machine sm;

    sg::state_machine<state>::state_tick_callback_t counter1_tick =
        [](state_machine& sm_, state_machine::current_state_details) {
            throw std::runtime_error("catch me");
            sm_.request_stop();
        };

    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);
    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE_THROWS(sm.future().get());
    REQUIRE_THROWS(sm.future_get_once());
}

TEST_CASE("sg::state_machine: check expections are caught in entry callback of starting state", "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };

    typedef sg::state_machine<state> state_machine;
    state_machine sm;
    std::atomic<int> counter{0};

    state_machine::state_change_callback_t counter1_entry =
        [](state_machine&, state_machine::state_change_details) {
            throw std::runtime_error("catch me");
        };

    sg::state_machine<state>::state_tick_callback_t counter1_tick =
        [&](state_machine&, state_machine::current_state_details) { counter++; };

    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);
    sm.add_entry_callback(state::add_to_counter1, counter1_entry);
    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE(counter == 0);  // the tick must not be called!
    REQUIRE_THROWS(sm.future().get());
    REQUIRE_THROWS(sm.future_get_once());

}

TEST_CASE("sg::state_machine: check expections are caught in exit callbacks of last state", "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };

    typedef sg::state_machine<state> state_machine;
    state_machine sm;
    std::atomic<int> counter{0};

    state_machine::state_change_callback_t counter1_exit =
        [](state_machine&, state_machine::state_change_details) {
            throw std::runtime_error("catch me");
        };

    sg::state_machine<state>::state_tick_callback_t counter1_tick =
        [&](state_machine& sm_, state_machine::current_state_details) {
        counter++;
        sm_.request_stop();
    };

    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);
    sm.add_exit_callback(state::add_to_counter1, counter1_exit);
    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE(counter == 1);  // the tick must not be called!
    REQUIRE_THROWS(sm.future().get());
    REQUIRE_THROWS(sm.future_get_once());
}

TEST_CASE("sg::state_machine: check expections are caught a state tick", "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };

    typedef sg::state_machine<state> state_machine;
    state_machine sm;
    std::atomic<int> counter{0};

    sg::state_machine<state>::state_tick_callback_t counter1_tick =
        [&](state_machine&, state_machine::current_state_details) {
        counter++;
        throw std::runtime_error("catch me");
    };

    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);
    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE(counter == 1);  // the tick must not be called!
    REQUIRE_THROWS(sm.future().get());
    REQUIRE_THROWS(sm.future_get_once());
}

TEST_CASE("sg::state_machine: check expections are caught in main state machine start tick", "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };

    typedef sg::state_machine<state> state_machine;
    state_machine sm;
    std::atomic<int> counter{0};

    state_machine::started_stopped_callback_t started_cb = [](state_machine&) {
        throw std::runtime_error("catch me");
    };

    state_machine::state_tick_callback_t counter1_tick =
        [&](state_machine&, state_machine::current_state_details) {
        counter++;
    };

    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);

    sm.set_machine_started_cb(started_cb);
    REQUIRE_THROWS(sm.start(state::add_to_counter1, std::chrono::seconds(1)));
    sm.wait_for_stop();

    REQUIRE(counter == 0);  // the tick must not be called!
    REQUIRE_NOTHROW(sm.future().get());
    REQUIRE_NOTHROW(sm.future_get_once());
}

TEST_CASE("sg::state_machine: check expections are caught in main state machine stop tick", "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };

    typedef sg::state_machine<state> state_machine;
    state_machine sm;
    std::atomic<int> counter{0};

    state_machine::started_stopped_callback_t stopped = [](state_machine&) {
        throw std::runtime_error("catch me");
    };

    state_machine::state_tick_callback_t counter1_tick =
        [&](state_machine& sm_, state_machine::current_state_details) {
        counter++;
        sm_.request_stop();
    };

    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);

    sm.set_machine_stopped_cb(stopped);
    sm.start(state::add_to_counter1, std::chrono::seconds(1));
    sm.wait_for_stop();

    REQUIRE(counter == 1);  // the tick must not be called!
    REQUIRE_THROWS(sm.future().get());
    REQUIRE_THROWS(sm.future_get_once());
}

TEST_CASE("sg::state_machine: check notify", "[sg::state_machine]") {
    enum class state {
        add_to_counter1,
    };

    typedef sg::state_machine<state> state_machine;
    state_machine sm;
    std::atomic<int> counter{0};
    std::binary_semaphore semaphore{0};

    state_machine::state_tick_callback_t counter1_tick = [&](state_machine& sm_,
                                                             state_machine::current_state_details) {
        if (counter == 0) {
            sm_.set_interval(std::chrono::minutes(10));
            semaphore.release();
        } else
          sm_.request_stop();
        counter++;
    };

    sm.add_state(state::add_to_counter1);
    sm.add_tick_callback(state::add_to_counter1, counter1_tick);
    sm.start(state::add_to_counter1, std::chrono::seconds(1));

    semaphore.acquire();
    sm.notify();
    sm.wait_for_stop();

    REQUIRE(counter == 2);  // the tick must not be called!
    REQUIRE_NOTHROW(sm.future().get());
    REQUIRE_NOTHROW(sm.future_get_once());
}
