#pragma once
#include <ngh/sim/SimTaskQueue.h>
#include <ngh/types/types.h>

namespace ngh::sim {
class SimTsSubscriberI {  // TODO(ANY): convert to concept
 public:
  virtual ~SimTsSubscriberI() = default;
  virtual void OnFill() = 0;
  virtual void OnOrder() = 0;
  virtual void OnAddAck() = 0;
  virtual void OnAddRej() = 0;
  virtual void OnCxlAck() = 0;
  virtual void OnCxlRej() = 0;
};

class SimTsPublisherI {
 public:
  SimTsPublisherI(SimTaskQueue& task_queue) : task_queue_(task_queue) {}
  virtual void Setup() = 0;
  void Reset() { subs_.clear(); }
  void Subscribe(data::UnixTimeMicro latency, SimTsSubscriberI& sub) {
    subs_.push_back({latency, sub});
  }

  template <typename... Args>
  void OnFill(Args&&... args) {
    for (auto& it : subs_) {
      task_queue_.PostAt(task_queue_.Now() + it.first,
                         [&sub = it.second, args...]() {
                           sub.OnFill(std::forward<Args>(args)...);
                         });
    }
  }

 private:
  SimTaskQueue& task_queue_;
  std::vector<std::pair<data::UnixTimeMicro, SimTsSubscriberI&>> subs_;
};

class SimQsSubscriberI {  // TODO(ANY): convert to concept
 public:
  // TODO(any): figureout the rule of X for this case
  virtual ~SimQsSubscriberI() = default;
  virtual void OnPacket(const data::Packet&) = 0;
};

class SimQsPublisher {
 public:
  SimQsPublisher(SimTaskQueue& task_queue, std::vector<data::Packet> pkts)
      : pkts_(pkts), task_queue_(task_queue) {}
  void Reset() { subs_.clear(); }
  void Subscribe(data::UnixTimeMicro latency, SimQsSubscriberI& sub) {
    subs_.push_back({latency, sub});
  }
  void Setup() {
    for (const auto& pkt : pkts_) {
      for (auto& it : subs_) {
        task_queue_.PostAt(pkt.qs_send_time + it.first,
                           [&]() { it.second.OnPacket(pkt); });
      }
    }
  }

 private:
  std::vector<data::Packet> pkts_;

  // external refereces
  SimTaskQueue& task_queue_;
  std::vector<std::pair<data::UnixTimeMicro, SimQsSubscriberI&>> subs_;
};
}  // namespace ngh::sim