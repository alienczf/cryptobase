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

}  // namespace ngh