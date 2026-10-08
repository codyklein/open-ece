#pragma once
#include "transient_execution.hpp"
#include <memory>
#include <thread>
namespace openece::gui {
// One owned numerical worker. No widgets/QObjects are accessed from its thread.
class TransientRunner {
  public:
    enum class Mode { running, paused, complete, cancelled, failed };
    struct Update {
        Mode mode;
        std::shared_ptr<const transient_core::Result> result;
        QString error; // Plain text, initialization may have no accepted prefix.
    };
    explicit TransientRunner(TransientExecution execution, bool single_step);
    ~TransientRunner();
    TransientRunner(const TransientRunner&) = delete;
    TransientRunner& operator=(const TransientRunner&) = delete;
    void resume();
    void pause();
    void step();
    void cancel();
    std::shared_ptr<const Update> take_update();

  private:
    struct Shared;
    std::shared_ptr<Shared> shared_;
    std::thread thread_;
    void command(int);
};
} // namespace openece::gui
