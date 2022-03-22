#pragma once
#include "ngh/types/time.h"

namespace ngh {

using Duration = std::chrono::nanoseconds;
using SysClock = std::chrono::system_clock;
using SysTime = SysClock::time_point;

using SteadyClock = std::chrono::steady_clock;
using SteadyTime = SteadyClock::time_point;

enum class Side : bool {
  kBuy = false,
  kSell = true,

  kBid = kBuy,
  kAsk = kSell,
};
constexpr bool IsBuy(Side v) { return v == Side::kBuy; }
constexpr bool IsSell(Side v) { return v == Side::kSell; }

constexpr bool IsBid(Side v) { return v == Side::kBid; }
constexpr bool IsAsk(Side v) { return v == Side::kAsk; }
constexpr Side operator~(Side v) { return IsBuy(v) ? Side::kSell : Side::kBuy; }

using OID = int64_t;
using Qty = double;
using Px = double;
struct Order {
  OID id;
  std::string type;
  std::string side;
  Px price;
  Qty size;
  std::string status;
  Qty filledSize;
  Qty remainingSize;
  bool reduceOnly;
  bool postOnly;
  bool ioc;
};
using Balances = std::map<std::string, Qty>;
using PriceBook = std::map<Px, Qty>;
using OrderBook = std::map<OID, Order>;

struct Ref {
  using Qty = int32_t;
  using Px = int32_t;

  int price_exp{0};
  int qty_exp{0};
  double hedge_ratio{NAN};
  double price_ratio{NAN};
  double px_offset{NAN};
  double mark_px{NAN};

  double norm_px(const Px px) const {
    return (static_cast<double>(px) - px_offset) / price_ratio;
  }
  template <typename T>
  double norm_qty(const T qty) const {
    return static_cast<double>(qty) / abs(hedge_ratio);
  }
  Side norm_side(const Side side) const {
    return hedge_ratio > 0 ? side : ~side;
  }
  Px denorm_px(const double px) const {
    return static_cast<Px>(px * price_ratio + px_offset);
  }
  Qty denorm_qty(const double qty) const {
    return static_cast<Qty>(qty * abs(hedge_ratio));
  }
  Side denorm_side(const Side side) const {
    return hedge_ratio > 0 ? side : ~side;
  }
};
using Refs = std::vector<Ref>;

namespace data::alc {
using UnixTimeNano = int64_t;
using UnixTimeMicro = int64_t;
using UnixTimeMilli = int64_t;
using EpochFracSecs = double;

using DurationNano = int64_t;
using DurationMicro = int64_t;
using DurationMilli = int64_t;
using DurationSecs = double;

using MD_ID = uint64_t;
using SeqNum = int32_t;
using VersionHdr = uint8_t;
using FlagsHdr = uint8_t;

// bitfields
struct __attribute__((packed)) StreamType {
  unsigned int value : 7;
  unsigned int : 1;
  bool full() const { return value == 0; }
  bool l1_snapshot() const { return value == 1; }
  friend std::ostream& operator<<(std::ostream& os, const StreamType& st) {
    if (st.full()) {
      os << "full";
    } else if (st.l1_snapshot()) {
      os << "l1_snapshot";
    } else {
      os << "unknown";
    }
    return os;
  }
};
struct __attribute__((packed)) Version {
  unsigned int value : 7;
  unsigned int : 1;
  friend std::ostream& operator<<(std::ostream& os, const Version& v) {
    os << static_cast<int>(v.value);
    return os;
  }
};
struct __attribute__((packed)) MsgTypeValue {
  unsigned int value : 7;
  unsigned int filtered : 1;
  friend std::ostream& operator<<(std::ostream& os, const MsgTypeValue& v) {
    if (v.filtered) {
      os << "filtered";
    } else {
      os << static_cast<int>(v.value);
    }
    return os;
  }
};

enum MsgTypeId {
  SnapshotMsg,           // v1, v2, v3
  DeltaMsg,              // v1, v2, v3
  TradeMsg,              // v2
  TradeListMsg,          // v1, v3
  InferredTradeListMsg,  // v3
  NumMsgTypes,           // v2, v3
  FundingRateMsg,
  UnknownMsg
};

struct __attribute__((packed)) Header {
  static constexpr int Version_FundingRate = 100;
  uint16_t _size;
  uint16_t stype;
  uint64_t ts;
  Version version;
  StreamType stream_type;
  uint16_t exch;
  uint32_t symbol;
  MsgTypeValue _msg_type;
  uint8_t qs_type;
  uint16_t size() const { return _size - sizeof(Header); }
  bool filtered() const { return _msg_type.filtered; }
  MsgTypeId msg_type() const {
    switch (_msg_type.value) {
      case 0:
        return SnapshotMsg;
      case 1:
        return DeltaMsg;
      case 2: {
        switch (version.value) {
          case 1:
            return TradeListMsg;
          case 2:
            return TradeMsg;
          case 3:
            return TradeListMsg;
        }
      }
      case 3:
        return (version.value == 2) ? NumMsgTypes : InferredTradeListMsg;
      case 4:
        return (version.value == Version_FundingRate) ? FundingRateMsg
                                                      : NumMsgTypes;
    }
    return UnknownMsg;
  }
  friend std::ostream& operator<<(std::ostream& os, const Header& h) {
    os << "size: " << h.size() << ", stype: " << h.stype << ", ts: " << h.ts
       << ", version: " << static_cast<int>(h.version.value)
       << ", stream_type: " << h.stream_type << ", exch: " << h.exch
       << ", symbol: " << h.symbol << ", msg_type: " << h.msg_type()
       << ", qs_type: " << h.qs_type;
    return os;
  }
};

struct __attribute__((packed)) BookHdrV2 {
  int16_t n_bids, n_asks, num_levels;
  int32_t qs_latency;  // qs_send_time - qs_recv_time
  SeqNum seqnum;
  UnixTimeMicro qs_send_time, exch_time;
  MD_ID md_id;
  friend std::ostream& operator<<(std::ostream& os, const BookHdrV2& h) {
    os << "n_bids: " << h.n_bids << ", n_asks: " << h.n_asks
       << ", num_levels: " << h.num_levels << ", qs_latency: " << h.qs_latency
       << ", seqnum: " << h.seqnum << ", qs_send_time: " << h.qs_send_time
       << ", exch_time: " << h.exch_time << ", md_id: " << h.md_id;
    return os;
  }
};

struct __attribute__((packed)) TradeHdrV3 {
  int8_t maker_side;
  int16_t n_trades;
  int32_t qs_latency;  // qs_send_time - qs_recv_time
  UnixTimeMicro qs_send_time, exch_time;
  MD_ID md_id;
  friend std::ostream& operator<<(std::ostream& os, const TradeHdrV3& h) {
    os << "maker_side: " << h.maker_side << ", n_trades: " << h.n_trades
       << ", qs_latency: " << h.qs_latency
       << ", qs_send_time: " << h.qs_send_time << ", exch_time: " << h.exch_time
       << ", md_id: " << h.md_id;
    return os;
  }
};

struct __attribute__((packed)) TradeV2 {
  int8_t maker_side;
  int32_t qs_latency;  // qs_send_time - qs_recv_time
  double prc, qty;
  UnixTimeMicro qs_send_time, exch_time;
  MD_ID md_id;
  friend std::ostream& operator<<(std::ostream& os, const TradeV2& t) {
    os << "maker_side: " << t.maker_side << ", qs_latency: " << t.qs_latency
       << ", prc: " << t.prc << ", qty: " << t.qty
       << ", qs_send_time: " << t.qs_send_time << ", exch_time: " << t.exch_time
       << ", md_id: " << t.md_id;
    return os;
  }
};

struct __attribute__((packed)) PriceLevelV2 {
  double prc, qty;
  int16_t cnt;
  friend std::ostream& operator<<(std::ostream& os, const PriceLevelV2& p) {
    os << "prc: " << p.prc << ", qty: " << p.qty << ", cnt: " << p.cnt;
    return os;
  }
};

struct __attribute__((packed)) TradeV3 {
  double prc, qty;
  friend std::ostream& operator<<(std::ostream& os, const TradeV3& t) {
    os << "prc: " << t.prc << ", qty: " << t.qty;
    return os;
  }
};

struct __attribute__((packed)) FundingRateV3 {
  int32_t qs_latency;  // qs_send_time - qs_recv_time
  UnixTimeMicro qs_send_time, exch_time;
  MD_ID md_id;
  double current_funding_rate;
  UnixTimeMicro current_funding_time;
  double next_funding_rate;
  UnixTimeMicro next_funding_time;
  double index_price;
  double mark_price;
  friend std::ostream& operator<<(std::ostream& os, const FundingRateV3& f) {
    os << "qs_latency: " << f.qs_latency << ", qs_send_time: " << f.qs_send_time
       << ", exch_time: " << f.exch_time
       << ", current_funding_rate: " << f.current_funding_rate
       << ", current_funding_time: " << f.current_funding_time
       << ", next_funding_rate: " << f.next_funding_rate
       << ", next_funding_time: " << f.next_funding_time
       << ", index_price: " << f.index_price
       << ", mark_price: " << f.mark_price;
    return os;
  }
};
}  // namespace data::alc

}  // namespace ngh