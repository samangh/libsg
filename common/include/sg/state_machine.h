#pragma once

#include "sg/notifiable_background_worker.h"
#include "callback.h"

#include <functional>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sg {

template <typename TState>
class state_machine {
   public:

    struct state_change_details {
        TState new_state;
        TState old_state;
        bool is_stopping_state = false;
    };

    struct current_state_details{
        TState current_state;
    };

    CREATE_CALLBACK(state_tick_callback_t, void, state_machine&, current_state_details details)
    CREATE_CALLBACK(state_change_callback_t, void, state_machine&, state_change_details details)
    CREATE_CALLBACK(started_stopped_callback_t, void, state_machine&)

    TState state() const { return m_current_state; }

    state_machine() =default;
    virtual ~state_machine() noexcept(false) = default;

    void add_state(TState state) {
        if (is_running()) throw std::runtime_error("can't add/remove states whilst running");
        m_states[state] = state_config();
    }

    void remove_state(TState state) {
        if (is_running()) throw std::runtime_error("can't add/remove states whilst running");
        m_states.erase(state);
    }

    void add_tick_callback(TState state, state_tick_callback_t cb) {
        m_states[state].m_cbs_ticks.push_back(cb);
    }

    void add_entry_callback(TState state, state_change_callback_t cb) {
        m_states[state].m_entry_cb.push_back(cb);
    }

    void add_exit_callback(TState state, state_change_callback_t cb) {
        m_states[state].m_exit_cb.push_back(cb);
    }

    void set_machine_started_cb(started_stopped_callback_t on_started) {
        if (is_running()) throw std::runtime_error("this state_machine is already running");
        m_on_start_cb = on_started;
    }

    void set_machine_stopped_cb(started_stopped_callback_t on_stopped) {
        if (is_running()) throw std::runtime_error("this state_machine is already running");
        m_on_stop_cb = on_stopped;
    }

    void start(TState initial_state, std::chrono::nanoseconds interval) {
        if (is_running())
            throw std::runtime_error("this state_machine is already running");

        m_current_state = initial_state;
        m_requested_state = initial_state;        

        auto tick_action_ = std::bind(&state_machine::tick_action, this, std::placeholders::_1);
        auto on_start_bind = std::bind(&state_machine::worker_on_start, this, std::placeholders::_1);
        auto on_stop_bind = std::bind(&state_machine::worker_on_stop, this, std::placeholders::_1);

        m_worker = std::make_unique<sg::notifiable_background_worker>(interval, tick_action_, on_start_bind, on_stop_bind);
        m_worker->start();
    }

    void request_stop() { m_worker->request_stop(); }
    void wait_for_stop() { m_worker->wait_for_stop(); }

    void stop() {
        request_stop();
        wait_for_stop();
    }

    bool is_running() const {
        return m_worker && m_worker->is_running();
    }

    void future_get_once() { m_worker->future_get_once(); }

    std::shared_future<void> future() const {
        return m_worker->future();
    }

    void set_state(TState state) {
        m_requested_state.store(state);
        m_worker->notify();
    }

    void notify() {
        m_worker->notify();
    }

    void set_interval(std::chrono::nanoseconds interval) {
        m_worker->set_interval(interval);
    }

    std::chrono::nanoseconds interval() const {
        return m_worker->interval();
    }


   private:
    started_stopped_callback_t m_on_start_cb {nullptr};
    started_stopped_callback_t m_on_stop_cb {nullptr};
    struct state_config {
        std::vector<state_tick_callback_t> m_cbs_ticks;
        std::vector<state_change_callback_t> m_entry_cb;
        std::vector<state_change_callback_t> m_exit_cb;
    };

    /* atomic because of set_state(..)  / state() */
    std::atomic<TState> m_current_state;
    std::atomic<TState> m_requested_state;


    bool just_started;
    std::map<TState, state_config> m_states;
    std::unique_ptr<sg::notifiable_background_worker> m_worker;

    void tick_action(sg::notifiable_background_worker*) {
        /* run entry action if just started */
        if (std::exchange(just_started, false))
        {
            state_change_details det{
                                     .new_state = m_current_state, .old_state = m_current_state};
            for (state_change_callback_t& entry_cbs : m_states.at(m_current_state).m_entry_cb)
                entry_cbs.invoke(*this, det);
        }

        /* if there is a state change */
        if (m_current_state != m_requested_state)
        {
            state_change_details det {.new_state=m_requested_state, .old_state=m_current_state};

            /* exit callbacks for old state */
            for (state_change_callback_t& exit_cbs: m_states.at(m_current_state).m_exit_cb)
                exit_cbs.invoke(*this, det);

            /* implement state change */
            m_current_state.store(m_requested_state);

            /* call entry cbs of new state */
            for (state_change_callback_t& entry_cbs: m_states.at(m_requested_state).m_entry_cb)
                entry_cbs.invoke(*this, det);
        }

        current_state_details det {.current_state = m_current_state};
        for (state_tick_callback_t& cbs: m_states.at(m_current_state).m_cbs_ticks)
            cbs.invoke(*this, det);
    }

    void worker_on_start (notifiable_background_worker*) {
        /* do overal on_start callback first, then the state's entry callbacks */
        if (m_on_start_cb)
            m_on_start_cb.invoke(*this);

        /* cause the worker to run the entry actions for the initial state */
        just_started=true;
    }
    void worker_on_stop (notifiable_background_worker*) {
        /* do the state's exit callbacks first, then the overall state machine one*/
        state_change_details det{
            .new_state = m_current_state, .old_state = m_current_state};
        for (state_change_callback_t& cbs: m_states.at(m_current_state).m_exit_cb)
            cbs.invoke(*this, det);

        if (m_on_stop_cb)
            m_on_stop_cb.invoke(*this);

    }

};

}
