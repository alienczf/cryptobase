#pragma once
#include <ngh/mkt/qshandler.h>
#include <ngh/sim/Messaging.h>
#include <ngh/sim/SimTaskQueue.h>

#include <map>

namespace ngh::sim {
using namespace data;
class NoImpactExchange : public SimTsPublisher {
  struct SimLevel {
    Px prc;
    std::vector<SimOrder> orders;
    size_t count() { return orders.size(); }
  };

 public:
  NoImpactExchange(SimTaskQueue& task_queue, std::vector<Packet> pkts)
      : SimTsPublisher(task_queue), task_queue_(task_queue) {
    // copy the packets and manipulate
    // TODO(any): not too happy with the double copy here but ohwell
    std::sort(pkts.begin(), pkts.end(), [](const Packet& a, const Packet& b) {
      return (a.exch_time < b.exch_time) || ((a.exch_time == b.exch_time) &&
                                             (a.qs_send_time < b.qs_send_time));
    });

    UnixTimeMicro now = 0;
    SeqNum seq_num = 0;
    std::vector<Packet> event{};
    for (const auto& pkt : pkts) {
      if (pkt.inferred || pkt.filtered ||
          ((pkt.levels.size()) && ((pkt.seq_num <= seq_num))))
        continue;
      if (event.size() && now != pkt.exch_time) {
        events_.push_back(event);
        event.clear();
      }
      event.push_back(pkt);
      now = pkt.exch_time;
      seq_num = std::max(seq_num, pkt.seq_num);
    }
  }

  virtual void Reset() final {
    SimTsPublisher::Reset();
    qs_.Reset();
    sim_levels_[0].clear();
    sim_levels_[1].clear();
  }

  void Setup() {
    Reset();
    for (const auto& event : events_) {
      task_queue_.PostAt(event.begin()->qs_send_time, [&]() {
        for (const auto& pkt : event) {
          qs_.OnPacketUnsafe(pkt);
          for (const auto& trd : pkt.trades) {
            handleTrade(trd);
          }
        }
        syncQueue();
      });
    }
  }

 private:
  void syncQueue() {
    for (auto& [px, level] : sim_levels_[0]) {
      Qty lvl_qty = 0.;
      if (auto it = qs_.levels[0].find(px); it != qs_.levels[0].end()) {
        lvl_qty = it->second;
      }
      for (auto& order : level.orders) {
        order.queue = std::min(order.queue, lvl_qty);
      }
    }
  }

  void handleTrade(Packet::Trade trd) {
    if (trd.side == MakerSide::B) {  // bids traded
      for (auto it = sim_levels_[0].rbegin(); it != sim_levels_[0].rend();
           ++it) {
        auto& orders = it->second.orders;
        if (it->first > trd.prc) {  // bids higher than traded prices
          for (auto& o : orders) {
            PubFill(o.FillOrder(o.qty));
            PubOrder(o);
          }
          it = std::make_reverse_iterator(
              sim_levels_[0].erase(std::next(it).base()));
        } else if (it->first == trd.prc) {
          for (auto oit = orders.rbegin();  // avoid invalidation
               oit != orders.rend();) {
            const auto fill_qty = oit->OnLevelTrade(trd.qty);
            if (fill_qty > 0) {
              PubFill(oit->FillOrder(fill_qty));
              PubOrder(*oit);
              if (oit->status == OrderStatus::DONE) {
                orders.erase(std::next(oit).base());
              }
            } else {
              ++oit;
            }
          }
          if (!orders.size()) {
            sim_levels_[0].erase(std::next(it).base());
          }
          break;
        } else {
          break;
        }
      }
    } else {  // asks traded
      for (auto it = sim_levels_[1].begin(); it != sim_levels_[1].end();) {
        auto& orders = it->second.orders;
        if (it->first < trd.prc) {  // asks lower than traded prices
          for (auto& o : orders) {
            PubFill(o.FillOrder(o.qty));
            PubOrder(o);
          }
          it = sim_levels_[1].erase(it);
        } else if (it->first == trd.prc) {
          for (auto oit = orders.rbegin();  // avoid invalidation
               oit != orders.rend();) {
            const auto fill_qty = oit->OnLevelTrade(trd.qty);
            if (fill_qty > 0) {
              PubFill(oit->FillOrder(fill_qty));
              PubOrder(*oit);
              if (oit->status == OrderStatus::DONE) {
                orders.erase(std::next(oit).base());
              }
            } else {
              ++oit;
            }
          }
          if (!orders.size()) {
            sim_levels_[1].erase(it);
          }
          break;
        } else {
          break;
        }
      }
    }
  }

  // cached for reuse
  std::vector<std::vector<Packet>> events_;
  // task queues
  SimTaskQueue& task_queue_;

  // book
  mkt::PktHandler qs_;
  std::map<Px, SimLevel> sim_levels_[2];
};
}  // namespace ngh::sim