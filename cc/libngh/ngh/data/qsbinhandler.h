#pragma once

#include <ngh/log.h>
#include <ngh/mkt/qshandler.h>
#include <ngh/types/types.h>

#include <boost/container/flat_map.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <fstream>
#include <iostream>
#include <string>

namespace ngh::data {
using namespace data::alc;

class BinLoader {
  using HeaderCb = std::function<void(const Header&)>;
  using LevelCb = std::function<void(const bool isBid, const BookHdrV2&,
                                     const PriceLevelV2&)>;
  using TradeV2Cb = std::function<void(const TradeV2&)>;
  using TradeV3Cb = std::function<void(const TradeHdrV3&, const TradeV3&)>;
  using FundingRateV3Cb = std::function<void(const FundingRateV3&)>;
  using UnknownCb = std::function<void(char*, size_t)>;
  using DoneCb = std::function<void()>;

 public:
  BinLoader(HeaderCb&& header_cb, LevelCb&& level_cb, TradeV2Cb&& trade_v2_cb,
            TradeV3Cb&& trade_v3_cb, FundingRateV3Cb&& funding_cb,
            UnknownCb&& unknown_cb, DoneCb&& done_cb)
      : header_cb_(std::move(header_cb)),
        level_cb_(std::move(level_cb)),
        trade_v2_cb_(std::move(trade_v2_cb)),
        trade_v3_cb_(std::move(trade_v3_cb)),
        funding_cb_(std::move(funding_cb)),
        unknown_cb_(std::move(unknown_cb)),
        done_cb_(std::move(done_cb)) {}

  template <typename T>
  void LoadOne(T& stream) {
    stream.read(reinterpret_cast<char*>(&header_), sizeof(Header));
    header_cb_(header_);
    switch (header_.msg_type()) {
      case SnapshotMsg:
      case DeltaMsg: {
        stream.read(reinterpret_cast<char*>(&bh_), sizeof(BookHdrV2));
        for (int i = 0; i < bh_.n_bids; ++i) {
          stream.read(reinterpret_cast<char*>(&pl_), sizeof(PriceLevelV2));
          level_cb_(true, bh_, pl_);
        }
        for (int i = 0; i < bh_.n_asks; ++i) {
          stream.read(reinterpret_cast<char*>(&pl_), sizeof(PriceLevelV2));
          level_cb_(false, bh_, pl_);
        }
        break;
      }
      case TradeMsg: {
        stream.read(reinterpret_cast<char*>(&t2_), sizeof(TradeV2));
        trade_v2_cb_(t2_);
        break;
      }
      case TradeListMsg:
      case InferredTradeListMsg: {
        stream.read(reinterpret_cast<char*>(&th_), sizeof(TradeHdrV3));
        for (int i = 0; i < th_.n_trades; ++i) {
          stream.read(reinterpret_cast<char*>(&t3_), sizeof(TradeV3));
          trade_v3_cb_(th_, t3_);
        }
        break;
      }
      case FundingRateMsg: {
        stream.read(reinterpret_cast<char*>(&fr_), sizeof(FundingRateV3));
        funding_cb_(fr_);
        break;
      }
      default: {
        stream.read(buf_, header_.size());
        unknown_cb_(buf_, header_.size());
      }
    }
    done_cb_();
  }

 private:
  // buffers
  Header header_{};
  BookHdrV2 bh_{};
  PriceLevelV2 pl_{};
  TradeV2 t2_{};
  TradeHdrV3 th_{};
  TradeV3 t3_{};
  FundingRateV3 fr_{};
  char buf_[std::numeric_limits<decltype(Header::_size)>::max()];

  // callback references
  HeaderCb header_cb_;
  LevelCb level_cb_;
  TradeV2Cb trade_v2_cb_;
  TradeV3Cb trade_v3_cb_;
  FundingRateV3Cb funding_cb_;
  UnknownCb unknown_cb_;
  DoneCb done_cb_;
};

std::stringstream LoadBin(const std::string& filename) {
  std::ifstream ifs(filename, std::ios::binary);
  if (!ifs) {
    throw std::runtime_error("failed to open file: " + filename);
  }
  boost::iostreams::filtering_streambuf<boost::iostreams::input> output;
  if (filename.find(".gz") != std::string::npos) {
    output.push(boost::iostreams::gzip_decompressor());
  }
  output.push(ifs);
  std::stringstream decompressed;
  boost::iostreams::copy(output, decompressed);
  return decompressed;
}

template <typename T>
boost::container::flat_map<UnixTimeMicro, std::function<void(T&)>>
LoadQsTaskQueue(std::stringstream&& ss) {
  boost::container::flat_map<UnixTimeMicro, std::function<void(T&)>> task_queue;
  std::map<UnixTimeMicro, std::pair<std::vector<std::function<void(T&)>>,
                                    std::vector<std::function<void(T&)>>>>
      cb_map;
  auto loader = BinLoader{
      [&](const Header& h) {},
      [&](const bool isBid, const BookHdrV2& bh, const PriceLevelV2& pl) {
        cb_map[bh.exch_time].second.push_back(
            [=](T& qs) { qs.OnLevel(isBid, bh, pl); });
      },
      [&](const TradeV2& t) {
        cb_map[t.exch_time].first.push_back([=](T& qs) { qs.OnTrade(t); });
      },
      [&](const TradeHdrV3& th, const TradeV3& t) {
        cb_map[th.exch_time].first.push_back([=](T& qs) { qs.OnTrade(th, t); });
      },
      [](const FundingRateV3& fr) {},
      [](char* buf, size_t sz) {
        LOGWRN("Unknown message: size:%zu, %s", sz, buf);
      },
      [&]() {}};

  std::cout << "loading callbacks:" << ss.eof() << std::endl;
  size_t i = 0;
  while (!ss.eof()) {
    loader.LoadOne(ss);
    ++i;
  }
  for (auto& [exchTs, v] : cb_map) {
    task_queue.emplace(exchTs, [v = std::move(v)](T& qs) {
      for (auto& cb : v.first) {
        cb(qs);
      }
      for (auto& cb : v.second) {
        cb(qs);
      }
      if (qs.levels[0].size() && qs.levels[1].size()) {
        const auto bid = qs.levels[0].rbegin();
        const auto ask = qs.levels[1].begin();
        LOGINF("%d,%lu,%lu,%lu,%d,%lu,%f,%f,%f,%f,%f,%f", qs.seq_num,
               qs.qs_send_time, qs.exch_time, qs.md_id,
               qs.last_trade_maker_side, qs.last_trade_time, qs.last_trade_prc,
               qs.last_trade_qty, bid->first, bid->second, ask->first,
               ask->second);
      }
    });
  }
  std::cout << "done loading packets:" << i << std::endl;

  return task_queue;
}

void LoadQsBinFile(std::string fn) {
  NanoLog::preallocate();
  NanoLog::setLogFile(std::getenv("LOG_FILE") ? std::getenv("LOG_FILE")
                                              : "/tmp/tmp.nanoclog");
  const auto task_queue = LoadQsTaskQueue<mkt::QsHandler>(LoadBin(fn));
  std::cout << "start playback:" << task_queue.size() << std::endl;
  mkt::QsHandler qs_handler{};
  LOGINF(
      "seq_num,qs_send_time,exch_time,md_id,last_trade_maker_side,last_trade_"
      "time,last_trade_prc,last_trade_qty,bid_prc,bid_qty,ask_prc,ask_qty");
  for (auto& [_, cb] : task_queue) {
    cb(qs_handler);
  }
  std::cout << "done playback" << std::endl;
}

void LoadQsBinFileUnordered(std::string fn) {
  NanoLog::preallocate();
  NanoLog::setLogFile(std::getenv("LOG_FILE") ? std::getenv("LOG_FILE")
                                              : "/tmp/tmp.nanoclog");
  auto decompressed = LoadBin(fn);
  mkt::QsHandler qs_handler{};
  auto loader = BinLoader{
      [&](const Header& h) {},
      [&](const bool isBid, const BookHdrV2& bh, const PriceLevelV2& pl) {
        qs_handler.OnLevel(isBid, bh, pl);
      },
      [&](const TradeV2& t) { qs_handler.OnTrade(t); },
      [&](const TradeHdrV3& th, const TradeV3& t) {
        qs_handler.OnTrade(th, t);
      },
      [](const FundingRateV3& fr) {},
      [](char* buf, size_t sz) {
        LOGWRN("Unknown message: size:%zu, %s", sz, buf);
      },
      [&]() {
        if (qs_handler.levels[0].size() && qs_handler.levels[1].size()) {
          const auto bid = qs_handler.levels[0].rbegin();
          const auto ask = qs_handler.levels[1].begin();
          LOGINF("%d,%lu,%lu,%lu,%d,%lu,%f,%f,%f,%f,%f,%f", qs_handler.seq_num,
                 qs_handler.qs_send_time, qs_handler.exch_time,
                 qs_handler.md_id, qs_handler.last_trade_maker_side,
                 qs_handler.last_trade_time, qs_handler.last_trade_prc,
                 qs_handler.last_trade_qty, bid->first, bid->second, ask->first,
                 ask->second);
        }
      }};
  std::cout << "start:" << decompressed.eof() << std::endl;
  size_t i = 0;
  LOGINF(
      "seq_num,qs_send_time,exch_time,md_id,last_trade_maker_side,last_trade_"
      "time,last_trade_prc,last_trade_qty,bid_prc,bid_qty,ask_prc,ask_qty");
  while (!decompressed.eof()) {
    loader.LoadOne(decompressed);
    ++i;
  }
  std::cout << "read packets:" << i << std::endl;
}

void DumpQsBinFile(std::string fn) {
  NanoLog::preallocate();
  NanoLog::setLogFile(std::getenv("LOG_FILE") ? std::getenv("LOG_FILE")
                                              : "/tmp/tmp.nanoclog");
  auto decompressed = LoadBin(fn);
  auto loader = BinLoader{
      [](const Header& h) {
        LOGINF(
            "Header: size: %d, stype: %d, ts: %lu, version: %d, stream_type: "
            "%d, exch: %d, symbol: %d, msg_type: %d, qs_type: %d",
            h.size(), h.stype, h.ts, static_cast<int>(h.version.value),
            static_cast<int>(h.stream_type.value), h.exch, h.symbol,
            h.msg_type(), h.qs_type);
      },
      [](const bool isBid, const BookHdrV2& bh, const PriceLevelV2& pl) {
        LOGINF(
            "Book[%d] n_bids: %d, n_asks: %d, num_levels: %d, "
            "qs_latency: %d, seqnum: %d, qs_send_time: %lu, exch_time: "
            "%lu, md_id: %lu, prc: %f, qty: %f, cnt: %d",
            isBid, bh.n_bids, bh.n_asks, bh.num_levels, bh.qs_latency,
            bh.seqnum, bh.qs_send_time, bh.exch_time, bh.md_id, pl.prc, pl.qty,
            pl.cnt);
      },
      [](const TradeV2& t) {
        LOGINF(
            "TradeV2: maker_side: %d, qs_latency: %d, qs_send_time: %lu, "
            "exch_time: %lu, md_id: %lu, prc: %f, qty: %f",
            t.maker_side, t.qs_latency, t.qs_send_time, t.exch_time, t.md_id,
            t.prc, t.qty);
      },
      [](const TradeHdrV3& th, const TradeV3& t) {
        LOGINF(
            "TradeV3: maker_side: %d, qs_latency: %d, qs_send_time: %lu, "
            "exch_time: %lu, md_id: %lu, prc: %f, qty: %f, n_trades: %d",
            th.maker_side, th.qs_latency, th.qs_send_time, th.exch_time,
            th.md_id, t.prc, t.qty, th.n_trades);
      },
      [](const FundingRateV3& fr) {
        LOGINF(
            "FundingRateV3: qs_latency: %d, qs_send_time: %lu, "
            "exch_time: %lu, md_id: %lu, current_funding_rate: %f, "
            "current_funding_time: %lu, next_funding_rate: %f, "
            "next_funding_time: %lu, index_price: %f, mark_price: %f",
            fr.qs_latency, fr.qs_send_time, fr.exch_time, fr.md_id,
            fr.current_funding_rate, fr.current_funding_time,
            fr.next_funding_rate, fr.next_funding_time, fr.index_price,
            fr.mark_price);
      },
      [](char* buf, size_t sz) {
        LOGWRN("Unknown message: size:%zu, %s", sz, buf);
      },
      []() { LOGDBG("end of packet"); }};
  std::cout << "start:" << decompressed.eof() << std::endl;
  size_t i = 0;
  while (!decompressed.eof()) {
    loader.LoadOne(decompressed);
    ++i;
  }
  std::cout << "read packets:" << i << std::endl;
}

}  // namespace ngh::data