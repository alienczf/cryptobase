#pragma once

#include <iostream>
#include <map>
#include <sstream>

#include "date.h"
#include "ngh/log.h"
#include "ngh/types/types.h"

namespace ngh::mkt {
using namespace data::alc;

class PktHandler {
 public:
  PktHandler() { Reset(); }
  void Reset() {
    seq_num = 0;
    qs_send_time = 0;
    exch_time = 0;
    md_id = 0;
    // book
    last_book_time = 0;  // exchange time
    levels[0].clear();
    levels[1].clear();
    // trade
    last_trade_time = 0;  // exchange time
    last_trade_maker_side = MakerSide::X;
    last_trade_prc = NAN;
    last_trade_qty = 0.;
  }

  void OnPacket(const Packet& pkt) {
    if (pkt.inferred || pkt.filtered ||
        ((pkt.levels.size()) && ((pkt.seq_num <= seq_num))))
      return;
    if (pkt.snapshot) Reset();

    const bool bookSeq = (seq_num == 0) || (pkt.seq_num == seq_num + 1);
    const bool bookFresh = pkt.exch_time > last_trade_time;
    const bool trdFresh = pkt.exch_time > last_book_time;
    if (!trdFresh && pkt.trades.size()) {
      LOGINF("got stale trade: current[%lu] received[%lu] md_id[%lu]",
             last_book_time, pkt.exch_time, pkt.md_id);
    }

    md_id = pkt.md_id;
    exch_time = std::max(exch_time, pkt.exch_time);
    qs_send_time = std::max(qs_send_time, pkt.qs_send_time);
    seq_num = std::max(seq_num, pkt.seq_num);

    if (pkt.levels.size()) {
      last_book_time = std::max(pkt.exch_time, last_book_time);
      if (bookSeq) {
        if (bookFresh) {
          for (const auto& lvl : pkt.levels) {
            OnLevel(lvl);
          }
        } else {
          LOGINF("got stale level: current[%lu] received[%lu] md_id[%lu]",
                 last_trade_time, pkt.exch_time, pkt.md_id);
        }
      } else {
        LOGWRN("got oos packet: current[%d] received[%d]", seq_num,
               pkt.seq_num);
      }
    }

    if (pkt.trades.size()) {
      last_trade_time = std::max(pkt.exch_time, last_trade_time);
      bool continued_trade = last_trade_time == pkt.exch_time;
      for (const auto& trd : pkt.trades) {
        OnTrade(trd, continued_trade);
        continued_trade = true;
      }
      if (trdFresh) {
        FlushBook(*pkt.trades.rbegin());
      } else {
        LOGINF("got stale trade: current[%lu] received[%lu] md_id[%lu]",
               last_book_time, pkt.exch_time, pkt.md_id);
      }
    }
  }

  void OnPacketUnsafe(const Packet& pkt) {
    // TODO(ANY): protected + friend exchange only?
    // no checks and etc.. only use if data is pre cleaned
    seq_num = pkt.seq_num;
    md_id = pkt.md_id;
    exch_time = pkt.exch_time;
    qs_send_time = pkt.qs_send_time;
    for (const auto& lvl : pkt.levels) {
      last_book_time = pkt.exch_time;
      OnLevel(lvl);
    }
    bool continued_trade = last_trade_time == pkt.exch_time;
    for (const auto& trd : pkt.trades) {
      OnTrade(trd, continued_trade);
      last_trade_time = pkt.exch_time;
      continued_trade = true;
    }
  }

  void OnLevel(const Packet::Level& lvl) {
    const auto idx = lvl.side == MakerSide::B ? 0 : 1;
    auto it = levels[idx].emplace(lvl.prc, 0).first;
    if (lvl.qty == 0.) {
      levels[idx].erase(it);
    } else {
      it->second = lvl.qty;
    }
  }

  void OnTrade(const Packet::Trade& trd, const bool continued_trade) {
    const auto prev_qty = last_trade_qty;
    last_trade_qty = trd.qty + continued_trade * last_trade_qty;
    last_trade_prc =
        (trd.prc * trd.qty + continued_trade *
                                 (isnan(last_trade_prc) ? 0. : last_trade_prc) *
                                 prev_qty) /
        last_trade_qty;
    last_trade_maker_side = trd.side;
  }

  void FlushBook(const Packet::Trade& trd) {
    if (trd.side == MakerSide::B) {  // bids traded
      for (auto it = levels[0].rbegin(); it != levels[0].rend();) {
        if (it->first > trd.prc) {  // bids higher than traded prices
          it =
              std::make_reverse_iterator(levels[0].erase(std::next(it).base()));
        } else if (it->first == trd.prc) {
          if (it->second <= 0.) {
            levels[0].erase(std::next(it).base());
          }
          break;
        } else {
          break;
        }
      }
    } else {  // asks traded
      for (auto it = levels[1].begin(); it != levels[1].end();) {
        if (it->first < trd.prc) {  // asks lower than traded prices
          it = levels[1].erase(it);
        } else if (it->first == trd.prc) {
          it->second -= trd.qty;
          if (it->second <= 0.) {
            levels[1].erase(it);
          }
          break;
        } else {
          break;
        }
      }
    }
  }

  // ordering
  SeqNum seq_num;
  UnixTimeMicro qs_send_time;
  UnixTimeMicro exch_time;
  MD_ID md_id;
  // book
  UnixTimeMicro last_book_time;  // exchange time
  PriceBook levels[2];
  // trade
  UnixTimeMicro last_trade_time;  // exchange time
  MakerSide last_trade_maker_side;
  Px last_trade_prc;
  Qty last_trade_qty;
};

}  // namespace ngh::mkt