#pragma once
#include "ngh/mkt/qshandler.h"
#include "ngh/sim/Messaging.h"
#include "ngh/sim/SimTaskQueue.h"  // TODO(ANY): interface

namespace ngh::sim {
class SimAlgo : public SimQsSubscriberI, public SimTsSubscriberI {
 public:
  SimAlgo(SimTaskQueue& task_queue) : task_queue_(task_queue) {
    LOGINF(
        "seq_num,qs_send_time,exch_time,md_id,upd_type,last_trade_maker_side,"
        "last_trade_time,last_trade_prc,last_trade_qty,bid_prc,bid_qty,ask_prc,"
        "ask_qty");
  }
  void Reset() { qs_.Reset(); }
  void Setup() {}

  virtual void OnFill(const SimFill&) final{};
  virtual void OnOrder(const SimOrder&) final{};
  virtual void OnAddRej(const SimOrder&, data::RejectReason) final{};
  virtual void OnCxlRej(const SimOrder&, data::RejectReason) final{};
  virtual void OnPacket(const data::Packet& pkt) final {
    qs_.OnPacket(pkt);
    if (qs_.levels[0].size() && qs_.levels[1].size()) {
      const auto bid = qs_.levels[0].rbegin();
      const auto ask = qs_.levels[1].begin();
      LOGINF("%d,%lu,%lu,%lu,PKT,%d,%lu,%f,%f,%f,%f,%f,%f", qs_.seq_num,
             task_queue_.Now(), pkt.exch_time, pkt.md_id,
             qs_.last_trade_maker_side, qs_.last_trade_time, qs_.last_trade_prc,
             qs_.last_trade_qty, bid->first, bid->second, ask->first,
             ask->second);
    }
  }

 private:
  mkt::PktHandler qs_;

  // external reference
  SimTaskQueue& task_queue_;
};
}  // namespace ngh::sim