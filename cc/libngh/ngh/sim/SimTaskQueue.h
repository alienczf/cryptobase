#pragma once

#include <ngh/types/types.h>

#include <map>

namespace ngh::sim {

class SimTaskQueue {
 public:
  using Task = std::function<void()>;
  void clear() { task_queue_.clear(); }
  void PostAt(data::UnixTimeMicro ts, Task&& cb) {
    ts = std::max(ts, now_ + 1);  // no timetravel
    while (task_queue_.find(ts) != task_queue_.end()) {
      ts++;  // avoid collision
    }
    task_queue_.emplace(ts, std::move(cb));
  }

  void RunUntilDone() {
    auto it = task_queue_.begin();
    while (it != task_queue_.end()) {
      now_ = it->first;
      it->second();
      it = task_queue_.erase(it);
    }
  }

  data::UnixTimeMicro Now() const { return now_; }

 private:
  data::UnixTimeMicro now_{0};
  std::map<data::UnixTimeMicro, Task> task_queue_;
};
}  // namespace ngh::sim
