#pragma once
#include <ngh/data/qsbinhandler.h>
#include <ngh/sim/Exchange.h>
#include <ngh/sim/SimTaskQueue.h>

#include <map>

namespace ngh::sim {

class SimTsSubscriberI {  // TODO(ANY): convert to concept
  virtual void OnFill() = 0;
  virtual void OnOrder() = 0;
  virtual void OnAddAck() = 0;
  virtual void OnAddRej() = 0;
  virtual void OnCxlAck() = 0;
  virtual void OnCxlRej() = 0;
};

class SimTsPublisher {
 public:
  SimTsPublisher(SimTaskQueue& task_queue) : task_queue_(task_queue) {}
  void Reset() { subs_.clear(); }
  void Subscribe(UnixTimeMicro latency, SimTsSubscriberI&& sub) {
    subs_.push_back({latency, sub});
  }

  template <typename... Args>
  void OnFill(Args&&... args) {
    for (auto& [lat, sub] : subs_) {
      task_queue_.PostAt(task_queue_.Now() + lat, [&]() {
        // sub.OnFill(std::forward<Args>(args)...);
      });
    }
  }

 private:
  SimTaskQueue& task_queue_;
  std::vector<std::pair<data::UnixTimeMicro, SimTsSubscriberI&>> subs_;
};

class SimQsPublisher {
 public:
  using DataCb = std::function<void(PriceBook*, std::vector<Packet::Trade>)>;
  SimQsPublisher(SimTaskQueue& task_queue, std::vector<Packet> pkts)
      : pkts_(pkts), task_queue_(task_queue) {}
  void Reset() {
    subs_.clear();
    qs_.Reset();
  }
  void Subscribe(UnixTimeMicro latency, DataCb&& sub) {
    subs_.push_back({latency, std::move(sub)});
  }
  void Setup() {
    for (const auto& pkt : pkts_) {
      task_queue_.PostAt(pkt.qs_send_time, [&]() {
        qs_.OnPacket(pkt);
        for (auto& [_, sub] : subs_) {
          // TODO(ZF): implement latency
          // will need to copy data.. sad =(
          sub(qs_.levels, pkt.trades);
        }
      });
    }
  }

 private:
  std::vector<Packet> pkts_;
  mkt::PktHandler qs_;

  // external refereces
  SimTaskQueue& task_queue_;
  std::vector<std::pair<data::UnixTimeMicro, DataCb>> subs_;
};

class SimSession {
 public:
  void LoadQsBinFiles(std::vector<std::string> files, uint16_t exch = 0,
                      uint32_t symbol = 0) {
    std::vector<Packet> pkts{};
    for (const auto& fn : files) {
      for (auto& pkt : LoadPacket(LoadBin(fn))) {
        pkts.push_back(pkt);  // TODO(ANY): extend not append
      }
    }
    if (!qs_pubs
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(exch, symbol),
                      std::forward_as_tuple(task_queue, pkts))
             .second |
        !ts_pubs
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(exch, symbol),
                      std::forward_as_tuple(task_queue, pkts))
             .second) {
      LOGWRN("data already loaded for %d %d. cowardly refusing to reload",
             symbol, exch);
    }
  }

  void Reset() {
    task_queue.clear();
    for (auto& [_, pub] : qs_pubs) {
      pub.Reset();
    }
    for (auto& [_, pub] : ts_pubs) {
      pub.Reset();
    }
  }

  void Setup() {
    for (auto& [_, pub] : qs_pubs) {
      pub.Setup();
    }
    for (auto& [_, pub] : ts_pubs) {
      pub.Setup();
    }
  }

  void Run() { task_queue.RunUntilDone(); }

  std::map<std::pair<uint16_t, uint32_t>, NoImpactExchange> ts_pubs;
  std::map<std::pair<uint16_t, uint32_t>, SimQsPublisher> qs_pubs;
  SimTaskQueue task_queue;
};

}  // namespace ngh::sim