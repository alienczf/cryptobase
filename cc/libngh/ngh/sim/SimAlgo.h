#pragma once
#include "ngh/sim/SimTaskQueue.h"  // TODO(ANY): interface
#include "ngh/types/types.h"

namespace ngh::sim {
class SimAlgo : SimTsSubscriberI {
 public:
  SimAlgo(SimTaskQueue& task_queue) : task_queue_(task_queue) {}
  void Reset() {}
  void Setup() template <typename... Args>
  void OnLevel(Args&&... args) {
    // qs_.OnLevel(std::forward<Args>(args)...);
  }
  template <typename... Args>
  void OnTrade(Args&&... args) {
    // qs_.OnTrade(std::forward<Args>(args)...);
    // flushSimLevels(qs_.last_trade_maker_side, qs_.last_trade_prc,
    //                qs_.last_trade_qty);
  }
  void OnPacket(PriceBook* levels, std::vector<Packet::Trade>& trades) {
    // while (!pending_queue_.empty()) {
    //   auto& [ts, cb] = pending_queue_.begin();
    //   if (ts > qs_.exch_time) {
    //     break;
    //   }
    //   cb(*this);
    //   pending_queue_.pop_front();
    // }
  }

 private:
  SimTaskQueue& task_queue_;
}
}  // namespace ngh::sim