#pragma once

#include <iostream>
#include <map>
#include <sstream>

#include "date.h"
#include "ngh/log.h"
#include "ngh/types/types.h"

namespace ngh::mkt {
using namespace data::alc;
class QsHandler {
 public:
  QsHandler(UnixTimeMicro trade_agg_window = 10000)
      : trade_agg_window(trade_agg_window) {
    LOGINF(
        "seq_num,qs_send_time,exch_time,md_id,last_trade_maker_side,last_trade_"
        "time,last_trade_prc,last_trade_qty,bid_prc,bid_qty,ask_prc,ask_qty");
  }

  void OnLevel(const bool isBid, const data::alc::BookHdrV2& hdr,
               const data::alc::PriceLevelV2& lvl) {
    if ((hdr.seqnum >= seq_num) && (hdr.exch_time >= exch_time)) {
      const auto idx = isBid ? 0 : 1;
      if (isnan(lvl.qty) || (lvl.qty == 0.)) {
        levels[idx].erase(lvl.prc);
      } else {
        levels[idx][lvl.prc] = lvl.qty;
      }
      md_id = hdr.md_id;
      exch_time = std::max(exch_time, hdr.exch_time);
      qs_send_time = std::max(qs_send_time, hdr.qs_send_time);
      seq_num = std::max(seq_num, hdr.seqnum);
    } else {
      LOGINF("got stale price: current[%lu] received[%lu] md_id[%lu]",
             exch_time, hdr.exch_time, hdr.md_id);
    }
  }

  void OnTrade(const MD_ID mdId, const UnixTimeMicro qsTs,
               const UnixTimeMicro exchTs, uint8_t makerSide, Px prc, Qty qty) {
    const bool continued_trade = ((last_trade_maker_side == makerSide) &&
                                  (exchTs - exch_time < trade_agg_window));
    const auto prev_qty = last_trade_qty;
    last_trade_qty = qty + continued_trade * last_trade_qty;
    last_trade_prc =
        (prc * qty + continued_trade *
                         (isnan(last_trade_prc) ? 0. : last_trade_prc) *
                         prev_qty) /
        last_trade_qty;
    last_trade_maker_side = makerSide;
    md_id = std::max(md_id, mdId);
    // these updates could be stale
    // TODO(ANY): retroactively shift this to l2 print
    exch_time = std::max(exch_time, exchTs);
    last_trade_time = std::max(last_trade_time, exchTs);
    qs_send_time = std::max(qs_send_time, qsTs);
    if (exchTs < exch_time) {
      LOGINF("got stale trade: current[%lu] received[%lu] md_id[%lu]",
             exch_time, exchTs, mdId);
    } else {
      FlushBook(makerSide, prc, qty);
    }
  }

  void OnTrade(const data::alc::TradeV2& t) {
    OnTrade(t.md_id, t.qs_send_time, t.exch_time, t.maker_side, t.prc, t.qty);
  }

  void OnTrade(const data::alc::TradeHdrV3& hdr, const data::alc::TradeV3& t) {
    OnTrade(hdr.md_id, hdr.qs_send_time, hdr.exch_time, hdr.maker_side, t.prc,
            t.qty);
  }

  void FlushBook(uint8_t makerSide, Px prc, Qty qty) {
    if (makerSide > 0) {  // bids traded
      for (auto it = levels[0].rbegin(); it != levels[0].rend(); ++it) {
        if (it->first > prc) {  // bids higher than traded prices
          levels[0].erase(std::next(it).base());
        } else if (it->first == prc) {
          it->second -= qty;
          if (it->second <= 0.) {
            levels[0].erase(std::next(it).base());
          }
          break;
        } else {
          break;
        }
      }
    } else {  // asks traded
      for (auto it = levels[1].begin(); it != levels[1].end(); ++it) {
        if (it->first < prc) {  // asks lower than traded prices
          levels[0].erase(it);
        } else if (it->first == prc) {
          it->second -= qty;
          if (it->second <= 0.) {
            levels[0].erase(it);
          }
          break;
        } else {
          break;
        }
      }
    }
  }

  void OnFunding(const data::alc::FundingRateV3& fr) {
    // TODO(ANY): implement
    LOGDBG(
        "FundingRateV3: qs_latency: %d, qs_send_time: %lu, "
        "exch_time: %lu, md_id: %lu, current_funding_rate: %f, "
        "current_funding_time: %lu, next_funding_rate: %f, "
        "next_funding_time: %lu, index_price: %f, mark_price: %f",
        fr.qs_latency, fr.qs_send_time, fr.exch_time, fr.md_id,
        fr.current_funding_rate, fr.current_funding_time, fr.next_funding_rate,
        fr.next_funding_time, fr.index_price, fr.mark_price);
  }

  void OnPacket() {
    if (levels[0].size() && levels[1].size()) {
      const auto bid = levels[0].rbegin();
      const auto ask = levels[1].begin();
      LOGINF("%d,%lu,%lu,%lu,%d,%lu,%f,%f,%f,%f,%f,%f", seq_num, qs_send_time,
             exch_time, md_id, last_trade_maker_side, last_trade_time,
             last_trade_prc, last_trade_qty, bid->first, bid->second,
             ask->first, ask->second);
    }
  }

  // config
  const UnixTimeMicro trade_agg_window;

  // ordering
  SeqNum seq_num{0};
  UnixTimeMicro qs_send_time{0};
  UnixTimeMicro exch_time{0};
  MD_ID md_id{0};
  // book
  PriceBook levels[2];
  // trade
  int8_t last_trade_maker_side{true};
  UnixTimeMicro last_trade_time{0};  // exchange time
  Px last_trade_prc{NAN};
  Qty last_trade_qty{0.};
};

}  // namespace ngh::mkt