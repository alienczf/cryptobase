#pragma once
#include <ngh/sim/SimTaskQueue.h>
#include <ngh/types/types.h>

namespace ngh::sim {

struct SimOrder {
  enum TIF : uint8_t { GTC, FAK, POST_ONLY };
  enum DONE_REASON : uint8_t {
    DONE_REASON_NONE,
    DONE_REASON_CANCEL,
    DONE_REASON_COD,
    DONE_REASON_STP,
    DONE_REASON_POST_ONLY,
    DONE_REASON_FULLY_FILLED,
    DONE_REASON_RESET
  };
  uint64_t id;
  uint64_t eid;
  Px prc;
  Qty qty;
  Qty queue;
  data::MakerSide side;
  TIF tif;
  data::OrderStatus status;
  DONE_REASON done_reason;
};

struct SimFill {
  uint64_t id;
  uint64_t eid;
  Px prc;
  Qty qty;  // always incremental
  Qty queue;
  data::MakerSide side;
  bool is_maker;
};

class SimTsSubscriberI {  // TODO(ANY): convert to concept
 public:
  virtual ~SimTsSubscriberI() = default;
  virtual void OnFill(const SimFill&) = 0;
  virtual void OnOrder(const SimOrder&) = 0;
  virtual void OnAddRej(const SimOrder&, data::RejectReason) = 0;
  virtual void OnCxlRej(const SimOrder&, data::RejectReason) = 0;
};

class SimTsPublisher {
 public:
  SimTsPublisher(SimTaskQueue& task_queue) : task_queue_(task_queue) {}
  virtual void Setup(){};
  virtual void Reset() { subs_.clear(); }
  void Subscribe(data::UnixTimeMicro latency, SimTsSubscriberI& sub) {
    subs_.push_back({latency, sub});
  }
// boo macros.. but this is more readable than some template magic
#define DEFINE_PUB_METHOD(msg)                                       \
  template <typename... Args>                                        \
  void Pub##msg(Args&&... args) {                                    \
    for (auto& it : subs_) {                                         \
      task_queue_.PostAt(task_queue_.Now() + it.first,               \
                         [&sub = it.second, args...]() {             \
                           sub.On##msg(std::forward<Args>(args)...); \
                         });                                         \
    }                                                                \
  }
  DEFINE_PUB_METHOD(Fill)
  DEFINE_PUB_METHOD(Order)
  DEFINE_PUB_METHOD(AddRej)
  DEFINE_PUB_METHOD(CxlRej)
#undef DEFINE_PUB_METHOD

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