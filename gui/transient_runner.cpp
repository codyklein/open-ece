#include "transient_runner.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
namespace openece::gui {
namespace {
enum Command { run, hold, single, terminate, shutdown };
}
struct TransientRunner::Shared {
    std::mutex mutex;
    std::condition_variable changed;
    int command;
    std::shared_ptr<const Update> update;
    bool terminal = false;
    std::uint64_t revision = 0;
};
TransientRunner::TransientRunner(TransientExecution e, bool one)
    : shared_(std::make_shared<Shared>()) {
    shared_->command = one ? single : run;
    thread_ = std::thread([s = shared_, e = std::move(e)]() mutable {
        namespace tr = transient_core;
        using Clock = std::chrono::steady_clock;
        auto publish = [&](Mode mode, tr::Simulation* sim, QString error = {}) {
            auto u = std::make_shared<Update>(
                Update{mode, sim ? std::make_shared<tr::Result>(sim->snapshot()) : nullptr,
                       std::move(error)});
            std::lock_guard lock(s->mutex);
            s->update = std::move(u); // Single-slot mailbox: no unbounded queued snapshots.
        };
        try {
            tr::Simulation simulation(std::move(e.circuit), std::move(e.request));
            auto next = Clock::now();
            while (true) {
                int cmd;
                {
                    std::unique_lock lock(s->mutex);
                    cmd = s->command;
                    if (cmd == hold) {
                        const auto revision = s->revision;
                        lock.unlock();
                        publish(Mode::paused, &simulation);
                        lock.lock();
                        s->changed.wait(
                            lock, [&] { return s->command != hold || s->revision != revision; });
                        cmd = s->command;
                        if (cmd == hold)
                            continue;
                    }
                    if (cmd == single)
                        s->command = hold;
                }
                if (cmd == terminate || cmd == shutdown) {
                    simulation.cancel();
                    if (cmd != shutdown)
                        publish(Mode::cancelled, &simulation);
                    break;
                }
                const auto status = simulation.step();
                if (status == tr::Status::complete || status == tr::Status::failed) {
                    publish(status == tr::Status::complete ? Mode::complete : Mode::failed,
                            &simulation);
                    break;
                }
                if (cmd != single && Clock::now() >= next) {
                    publish(Mode::running, &simulation);
                    next = Clock::now() + std::chrono::milliseconds(200);
                }
            }
        } catch (const tr::Error& error) {
            QString detail = QString::fromUtf8(error.what());
            for (auto n : error.nodes())
                detail += "\nNode ID: " + QString::number(n.value);
            for (auto c : error.components())
                detail += "\nComponent ID: " + QString::number(c.value);
            publish(Mode::failed, nullptr,
                    "Initialization failed (code " +
                        QString::number(static_cast<int>(error.code())) + "): " + detail);
        } catch (const std::exception&) {
            publish(Mode::failed, nullptr,
                    "Execution could not allocate or prepare its owned numerical state.");
        }
        std::lock_guard lock(s->mutex);
        s->terminal = true;
    });
}
TransientRunner::~TransientRunner() {
    command(shutdown);
    if (thread_.joinable())
        thread_.join(); // At most the current atomic initialization/interval.
}
void TransientRunner::command(int cmd) {
    {
        std::lock_guard lock(shared_->mutex);
        if (!shared_->terminal) {
            shared_->command = cmd;
            ++shared_->revision;
        }
    }
    shared_->changed.notify_all();
}
void TransientRunner::resume() { command(run); }
void TransientRunner::pause() { command(hold); }
void TransientRunner::step() { command(single); }
void TransientRunner::cancel() { command(terminate); }
std::shared_ptr<const TransientRunner::Update> TransientRunner::take_update() {
    std::lock_guard lock(shared_->mutex);
    return std::exchange(shared_->update, nullptr);
}
} // namespace openece::gui
