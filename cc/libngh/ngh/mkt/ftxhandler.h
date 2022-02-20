#pragma once

#include <iostream>
#include <map>
#include <sstream>

#include "date.h"
#include "ngh/types/types.h"
#include "simdjson.h"

namespace ngh::mkt {
class L2StateTracker {
 public:
  template <typename T>
  void onTrade(T trades) {
    for (auto trade : trades.get_array()) {
      lastTradePx = Px(trade["price"]);
      lastTradeQty = Qty(trade["size"]) * (trade["side"] == "buy" ? 1 : -1);
      lastTradeIsLiquidation = bool(trade["liquidation"]);
      std::chrono::system_clock::time_point tp;
      std::string_view ts = trade["time"];
      std::istringstream ss{std::string(ts)};
      ss >> date::parse("%FT%TZ", tp);
      lastTradeTs =
          std::chrono::duration<double>(tp.time_since_epoch()).count();
      lastTradeTs = lastTs;
    }
  }

  template <typename T>
  void onL2(T l2) {
    lastTs = l2["time"];
    for (auto upd : l2["bids"].get_array()) {
      auto it = upd.get_array().value().begin().value();
      const auto px = -double(*it);  // highest value up top
      const Qty qty = *(++it);
      if (qty == 0.) {
        levels[0].erase(px);
      } else {
        levels[0][px] = qty;
      }
    }

    for (auto upd : l2["asks"].get_array()) {
      auto it = upd.get_array().value().begin().value();
      Px px = *it;
      Qty qty = *(++it);
      if (qty == 0.) {
        levels[1].erase(px);
      } else {
        levels[1][px] = qty;
      }
    }
    // TODO(ANY): checksum?
  }

  template <typename T>
  void onOrder(T o) {
    OID oid = o["id"];
    if (o["status"] == "closed") {
      orders.erase(oid);
    } else {
      auto& order =
          orders
              .emplace(std::piecewise_construct, std::forward_as_tuple(oid),
                       std::forward_as_tuple())
              .first->second;
      order.id = oid;
      order.type = std::string_view(o["type"]);
      order.side = std::string_view(o["side"]);
      order.price = o["price"];
      order.size = o["size"];
      order.status = std::string_view(o["status"]);
      order.filledSize = o["filledSize"];
      order.remainingSize = o["remainingSize"];
      order.reduceOnly = o["reduceOnly"];
      order.postOnly = o["postOnly"];
      order.ioc = o["ioc"];
    }
  }

  PriceBook& getBids() { return levels[0]; }
  PriceBook& getAsks() { return levels[1]; }

  // public states
  double lastTs{0.};
  PriceBook levels[2];

  // trade
  double lastTradeTs{0.};
  Px lastTradePx{NAN};
  Qty lastTradeQty{0.};
  bool lastTradeIsLiquidation{false};

  // private states
  OrderBook orders;
};

class FtxHandler {
  static constexpr auto BUFFER_SIZE = 64 * 1024;

 public:
  FtxHandler() { parser_.allocate(BUFFER_SIZE); }
  void reset() {
    balances.clear();
    books_.clear();
  }
  L2StateTracker& getBook(const std::string_view s) {
    return books_
        .emplace(std::piecewise_construct, std::forward_as_tuple(s),
                 std::forward_as_tuple())
        .first->second;
  }

  bool onMessage(const std::string& s) {
    strcpy(buffer_, s.c_str());
    doc_ = parser_.iterate(buffer_, s.size(), BUFFER_SIZE);
    std::string_view type = doc_["type"];
    if (type == "update") {
      std::string_view channel = doc_["channel"];
      if (channel == "trades") {
        std::string_view market = doc_["market"];
        getBook(market).onTrade(doc_["data"]);
      } else if (channel == "orderbook") {
        std::string_view market = doc_["market"];
        auto& book = getBook(market);
        book.onL2(doc_["data"]);
      } else if (channel == "fills") {
        onFill(doc_["data"]);
      } else if (channel == "orders") {
        getBook(doc_["data"]["market"]).onOrder(doc_["data"]);
      }
      return true;
    } else if (type == "partial") {  // must be orderbook
      std::string_view market = doc_["market"];
      auto& book =
          books_
              .emplace(std::piecewise_construct, std::forward_as_tuple(market),
                       std::forward_as_tuple())
              .first->second;
      book.onL2(doc_["data"]);
      return true;
    }
    return false;
  }
  Balances balances;

 private:
  template <typename T>
  void onFill(T fill) {
    std::cout << "got fill:" << fill << std::endl;
    const auto sign = (fill["side"] == "buy") ? 1 : -1;
    const auto qty = Qty(fill["size"]) * sign;
    const std::string mkt{std::string_view(fill["market"])};
    size_t offset = mkt.find('/');
    const auto base =
        (offset != std::string::npos) ? mkt.substr(0, offset) : mkt;
    balances
        .emplace(std::piecewise_construct, std::forward_as_tuple(base),
                 std::forward_as_tuple(0.))
        .first->second += qty;
    const auto paid =
        (offset != std::string::npos) ? mkt.substr(offset) : "USD";
    const auto ntl = qty * double(fill["price"]);
    balances
        .emplace(std::piecewise_construct, std::forward_as_tuple(paid),
                 std::forward_as_tuple(0.))
        .first->second -= ntl;
    // fill["fee"];
    // fill["orderId"];
    // fill["time"];
    // fill["type"];
    // fill["tradeId"];
  }

  simdjson::ondemand::parser parser_{BUFFER_SIZE};
  simdjson::ondemand::document doc_;
  char buffer_[BUFFER_SIZE];

  std::map<std::string, L2StateTracker> books_;
};

}  // namespace ngh::mkt