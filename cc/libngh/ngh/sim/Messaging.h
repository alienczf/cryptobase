#pragma once
#include <ngh/sim/SimTaskQueue.h>
#include <ngh/types/types.h>

namespace ngh::sim {

enum TIF : uint8_t { GTC = 0, FAK = 1, POST = 2 };
struct SimReqSubmit {
  TIF tif;
  uint64_t oid;
  data::MakerSide side;
  Px prc;
  Qty qty;
};

struct SimReqCancel {
  uint64_t oid;
  // not needed but we use this to speed up lookup
  data::MakerSide side;
  Px prc;
};

struct SimReq {
  enum Type : uint8_t { SUBMIT = 0, CANCEL = 1 };
  Type type;
  data::UnixTimeMicro ts;
  SimReqSubmit submit;
  SimReqCancel cancel;
};
using Reqs = std::vector<SimReq>;

struct SimFill {
  uint64_t id;
  Px prc;
  Qty qty;  // always incremental
  data::MakerSide side;
  bool is_maker;
};

struct SimOrder {
  enum DONE_REASON : uint8_t {
    DONE_REASON_NONE,
    DONE_REASON_CANCEL,
    DONE_REASON_COD,
    DONE_REASON_STP,
    DONE_REASON_POST_ONLY,
    DONE_REASON_FAK,
    DONE_REASON_FULLY_FILLED,
    DONE_REASON_RESET
  };
  uint64_t id;
  Px prc;
  Qty qty;
  Qty queue;  // used by exchange to track qspot
  data::MakerSide side;
  TIF tif;
  data::OrderStatus status;
  DONE_REASON done_reason;

  SimFill FillOrder(const Qty fill_qty) {
    qty -= std::min(fill_qty, qty);
    if (qty == 0.) {
      status = data::OrderStatus::DONE;
      done_reason = DONE_REASON_FULLY_FILLED;
    }
    return {
        .id = id, .prc = prc, .qty = fill_qty, .side = side, .is_maker = true};
  }
  Qty OnLevelTrade(const Qty trd_qty) {
    const auto queue_trd = std::min(trd_qty, queue);
    const auto fill_qty = trd_qty - queue_trd;
    queue -= queue_trd;
    return fill_qty;
  }
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
  virtual ~SimTsPublisher() = default;
  virtual void Setup(){};
  virtual void Reset() { subs_.clear(); }
  void Subscribe(data::UnixTimeMicro latency, SimTsSubscriberI& sub) {
    subs_.push_back({latency, sub});
  }

  // TODO(ANY): need to inject latency here
  virtual void ReqSubmit(const SimReqSubmit&, const data::UnixTimeMicro) = 0;
  virtual void ReqCancel(const SimReqCancel&, const data::UnixTimeMicro) = 0;

// boo macros.. but this is more readable than some template magic
// https://stackoverflow.com/questions/53033486/capturing-a-copy-of-parameter-pack
#define DEFINE_PUB_METHOD(msg)                                                \
  template <typename... Args>                                                 \
  void Pub##msg(Args&&... args) {                                             \
    for (auto& it : subs_) {                                                  \
      task_queue_.PostAt(task_queue_.Now() + it.first,                        \
                         [tup = std::make_tuple(std::forward<Args>(args)...), \
                          &sub = it.second]() {                               \
                           auto doPub = [&](auto&... params) {                \
                             sub.On##msg(std::move(params)...);               \
                           };                                                 \
                           std::apply(doPub, tup);                            \
                         });                                                  \
    }                                                                         \
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