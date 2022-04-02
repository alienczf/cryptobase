#pragma once
#include <ngh/mkt/qshandler.h>
#include <ngh/sim/Messaging.h>
#include <ngh/sim/SimTaskQueue.h>

#include <map>

namespace ngh::sim {
using namespace data;
class NoImpactExchange : public SimTsPublisher {
  struct SimLevel {
    std::vector<SimOrder> orders;
    size_t count() { return orders.size(); }
  };

 public:
  NoImpactExchange(SimTaskQueue& task_queue, std::vector<Packet> pkts)
      : SimTsPublisher(task_queue), task_queue_(task_queue) {
    // TODO(ANY): move out of this place
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
    UnixTimeMicro now = std::numeric_limits<UnixTimeMicro>::max();
    for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
      // go backwards in time to fix out of ordered packets from exchange
      const auto& event = *it;
      now = std::min(event.begin()->qs_send_time, now - 1);
      task_queue_.PostAt(now, [&]() {
        Packet::Trade agg_buy{
            .prc = -INFINITY, .qty = 0, .side = data::MakerSide::S};
        Packet::Trade agg_sell{
            .prc = INFINITY, .qty = 0, .side = data::MakerSide::B};
        for (const auto& pkt : event) {
          if (pkt.snapshot) qs_.Reset();
          qs_.OnPacketUnsafe(pkt);
          for (const auto& trd : pkt.trades) {
            if ((trd.side == data::MakerSide::S) && (trd.prc > agg_buy.prc)) {
              agg_buy = trd;
            } else if ((trd.side == data::MakerSide::B) &&
                       (trd.prc < agg_sell.prc)) {
              agg_sell = trd;
            }
          }
        }
        if (agg_buy.qty > 0.) handleTrade(agg_buy);
        if (agg_sell.qty > 0.) handleTrade(agg_sell);
        syncQueue();

        if (qs_.levels[0].size() && qs_.levels[1].size()) {
          const auto bid = qs_.levels[0].rbegin();
          const auto ask = qs_.levels[1].begin();
          LOGINF("%d,%lu,%lu,%lu,EXCH,%d,%lu,%f,%f,%f,%f,%f,%f", qs_.seq_num,
                 task_queue_.Now(), qs_.exch_time, qs_.md_id,
                 qs_.last_trade_maker_side, qs_.last_trade_time,
                 qs_.last_trade_prc, qs_.last_trade_qty, bid->first,
                 bid->second, ask->first, ask->second);
        }
      });
    }
  }

  virtual void ReqSubmit(const SimReqSubmit& req,
                         const data::UnixTimeMicro ts = 0) final {
    task_queue_.PostAt(ts, [=]() {
      LOGINF("SimSubmit:SUBMIT_REQ,%lu,%lu,%lu", req.oid, task_queue_.Now(),
             qs_.md_id);
      SimOrder o{.id = req.oid,
                 .prc = req.prc,
                 .qty = req.qty,
                 .queue = 0.,
                 .side = req.side,
                 .tif = req.tif};
      const Qty opp_px =
          (req.side == MakerSide::B)
              ? (qs_.levels[1].empty() ? INFINITY
                                       : qs_.levels[1].begin()->first)
              : (qs_.levels[0].empty() ? -INFINITY
                                       : qs_.levels[0].rbegin()->first);
      const auto is_crossing =
          (req.side == MakerSide::B) ? req.prc >= opp_px : req.prc <= opp_px;
      if (is_crossing) {
        if (req.tif == POST) {
          o.qty = 0;
          o.status = data::DONE;
          o.done_reason = SimOrder::DONE_REASON_POST_ONLY;
          PubOrder(o);
          LOGINF("SimSubmit:DONE,%lu,%lu,%lu", o.id, task_queue_.Now(),
                 qs_.md_id);
          LOGINF("SimSubmit:POST_CROSSED,%lu,%lu,%lu", o.id, task_queue_.Now(),
                 qs_.md_id);
        } else {
          auto fill = o.FillOrder(o.qty);
          fill.prc = opp_px;
          PubFill(fill);
          PubOrder(o);
          LOGINF("SimSubmit:DONE,%lu,%lu,%lu", o.id, task_queue_.Now(),
                 qs_.md_id);
          LOGINF("FILL:%lu,%lu,%lu,%f,%f,%f", o.id, task_queue_.Now(),
                 qs_.md_id, fill.prc, fill.qty, o.qty);
        }
      } else {
        if (req.tif == FAK) {
          o.qty = 0;
          o.status = data::DONE;
          o.done_reason = SimOrder::DONE_REASON_FAK;
          PubOrder(o);
          LOGINF("SimSubmit:DONE,%lu,%lu,%lu", req.oid, task_queue_.Now(),
                 qs_.md_id);
        } else {
          sim_levels_[(req.side == MakerSide::B) ? 0 : 1]
              .emplace(std::piecewise_construct, std::forward_as_tuple(req.prc),
                       std::forward_as_tuple())
              .first->second.orders.push_back(o);
          PubOrder(o);
          LOGINF("SimSubmit:DONE,%lu,%lu,%lu", req.oid, task_queue_.Now(),
                 qs_.md_id);
        }
      }
    });
  }

  virtual void ReqCancel(const SimReqCancel& req,
                         const data::UnixTimeMicro ts = 0) final {
    task_queue_.PostAt(ts, [=]() {
      LOGINF("SimCancel:CANCEL_REQ,%lu,%lu,%lu", req.oid, task_queue_.Now(),
             qs_.md_id);
      auto& levels = sim_levels_[(req.side == MakerSide::B) ? 0 : 1];
      if (auto it = levels.find(req.prc); it != levels.end()) {
        auto& orders = it->second.orders;
        for (auto oit = orders.begin(); oit != orders.end(); it++) {
          if (oit->id == req.oid) {
            oit->status = data::DONE;
            oit->done_reason = SimOrder::DONE_REASON_CANCEL;
            oit->qty = 0.;
            PubOrder(*oit);
            LOGINF("SimCancel:DONE,%lu,%lu,%lu", oit->id, task_queue_.Now(),
                   qs_.md_id);
            orders.erase(oit);
            if (orders.empty()) {
              levels.erase(it);
            }
            return;
          }
        }
      }
      LOGINF("SimCancel:CXL_FAIL,%lu,%lu,%lu", req.oid, task_queue_.Now(),
             qs_.md_id);
    });
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
      for (auto it = sim_levels_[0].rbegin(); it != sim_levels_[0].rend();) {
        auto& orders = it->second.orders;
        if (it->first > trd.prc) {  // bids higher than traded prices
          for (auto& o : orders) {
            const auto fill = o.FillOrder(o.qty);
            PubFill(fill);
            PubOrder(o);
            LOGINF("TRADE:FULL_FILL,%lu,%lu,%lu", o.id, task_queue_.Now(),
                   qs_.md_id);
            LOGINF("FILL:FILL:%lu,%lu,%lu,%f,%f,%f", o.id, task_queue_.Now(),
                   qs_.md_id, fill.prc, fill.qty, o.qty);
          }
          it = std::make_reverse_iterator(
              sim_levels_[0].erase(std::next(it).base()));
        } else if (it->first == trd.prc) {
          for (auto oit = orders.rbegin();
               oit != orders.rend();) {  // avoid invalidation
            const auto fill_qty = oit->OnLevelTrade(trd.qty);
            if (fill_qty > 0) {
              const auto fill = oit->FillOrder(fill_qty);
              PubFill(fill);
              PubOrder(*oit);
              LOGINF("TRADE:FILL,%lu,%lu,%lu", oit->id, task_queue_.Now(),
                     qs_.md_id);
              LOGINF("FILL:FILL:%lu,%lu,%lu,%f,%f,%f", oit->id,
                     task_queue_.Now(), qs_.md_id, fill.prc, fill.qty,
                     oit->qty);
              if (oit->status == OrderStatus::DONE) {
                oit = std::make_reverse_iterator(
                    orders.erase(std::next(oit).base()));
                continue;
              }
            }
            ++oit;
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
            const auto fill = o.FillOrder(o.qty);
            PubFill(fill);
            PubOrder(o);
            LOGINF("TRADE:FULL_FILL,%lu,%lu,%lu", o.id, task_queue_.Now(),
                   qs_.md_id);
            LOGINF("FILL:%lu,%lu,%lu,%f,%f,%f", o.id, task_queue_.Now(),
                   qs_.md_id, fill.prc, fill.qty, o.qty);
          }
          it = sim_levels_[1].erase(it);
        } else if (it->first == trd.prc) {
          for (auto oit = orders.rbegin();
               oit != orders.rend();) {  // avoid invalidation
            const auto fill_qty = oit->OnLevelTrade(trd.qty);
            if (fill_qty > 0) {
              const auto fill = oit->FillOrder(fill_qty);
              PubFill(fill);
              PubOrder(*oit);
              LOGINF("TRADE:FILL,%lu,%lu,%lu,%f,%d", oit->id, task_queue_.Now(),
                     qs_.md_id, oit->qty, static_cast<int>(oit->status));
              LOGINF("FILL:%lu,%lu,%lu,%f,%f,%f", oit->id, task_queue_.Now(),
                     qs_.md_id, fill.prc, fill.qty, oit->qty);
              if (oit->status == OrderStatus::DONE) {
                oit = std::make_reverse_iterator(
                    orders.erase(std::next(oit).base()));
                continue;
              }
            }
            ++oit;
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