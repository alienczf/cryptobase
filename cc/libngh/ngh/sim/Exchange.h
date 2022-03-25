#pragma once
#include <ngh/mkt/qshandler.h>
#include <ngh/sim/Messaging.h>
#include <ngh/sim/SimTaskQueue.h>

#include <boost/circular_buffer.hpp>
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
    pending_queue_.clear();
    sim_levels_[0].clear();
    sim_levels_[1].clear();
  }

  void Setup() {
    Reset();
    for (const auto& event : events_) {
      task_queue_.PostAt(event.begin()->qs_send_time, [&]() {
        Packet::Trade last_trd{};
        for (const auto& pkt : event) {
          qs_.OnPacketUnsafe(pkt);
          for (const auto& trd : pkt.trades) {
            last_trd = trd;
          }
        }
        if (last_trd.qty > 0) {
          handleTrade(last_trd);
        }
      });
    }
  }

 private:
  void handleTrade(Packet::Trade trd) {
    if (trd.side == MakerSide::B) {  // bids traded
      for (auto it = sim_levels_[0].rbegin(); it != sim_levels_[0].rend();
           ++it) {
        if (it->first > trd.prc) {  // bids higher than traded prices
          sim_levels_[0].erase(std::next(it).base());
          // } else if (it->first == trd.prc) {
          //   it->second -= trd.qty;
          //   if (it->second <= 0.) {
          //     sim_levels_[0].erase(std::next(it).base());
          //   }
          //   break;
        } else {
          break;
        }
      }
    } else {  // asks traded
      for (auto it = sim_levels_[1].begin(); it != sim_levels_[1].end(); ++it) {
        if (it->first < trd.prc) {  // asks lower than traded prices
          sim_levels_[0].erase(it);
          // } else if (it->first == trd.prc) {
          //   it->second -= trd.qty;
          //   if (it->second <= 0.) {
          //     sim_levels_[0].erase(it);
          //   }
          //   break;
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
  // orderbook
  boost::circular_buffer_space_optimized<
      std::pair<UnixTimeMicro, std::function<void(NoImpactExchange&)>>>
      pending_queue_{64};  // increase as needed
};
}  // namespace ngh::sim