#pragma once
#include <ngh/data/qsbinhandler.h>
#include <ngh/sim/Exchange.h>
#include <ngh/sim/Messaging.h>
#include <ngh/sim/SimAlgo.h>
#include <ngh/sim/SimTaskQueue.h>

#include <map>

namespace ngh::sim {

class SimSession {
 public:
  void LoadQsBinFiles(std::vector<std::string> files, uint16_t exch,
                      uint32_t symbol) {
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
    task_queue.Reset();
    for (auto& [_, pub] : qs_pubs) {
      pub.Reset();
    }
    for (auto& [_, pub] : ts_pubs) {
      pub.Reset();
    }
    algo_subs.clear();
  }

  void Setup() {
    NanoLog::preallocate();
    NanoLog::setLogFile(std::getenv("LOG_FILE") ? std::getenv("LOG_FILE")
                                                : "/tmp/tmp.nanoclog");
    for (auto& [_, pub] : qs_pubs) {
      pub.Setup();
    }
    for (auto& [_, pub] : ts_pubs) {
      pub.Setup();
    }
  }

  void AddAlgo(uint16_t exch, uint32_t symbol, data::UnixTimeMicro latency) {
    // TODO(ANY): fetch latency config map
    algo_subs.push_back({task_queue});
    auto& algo = *algo_subs.rbegin();
    if (auto it = ts_pubs.find({exch, symbol}); it != ts_pubs.end()) {
      it->second.Subscribe(latency, algo);
    } else {
      LOGWRN("no ts_pubs for %d %d", symbol, exch);
    }
    if (auto it = qs_pubs.find({exch, symbol}); it != qs_pubs.end()) {
      it->second.Subscribe(latency, algo);
    } else {
      LOGWRN("no qs_pubs for %d %d", symbol, exch);
    }
  }

  void Run() { task_queue.RunUntilDone(); }

  std::map<std::pair<uint16_t, uint32_t>, NoImpactExchange> ts_pubs;
  std::map<std::pair<uint16_t, uint32_t>, SimQsPublisher> qs_pubs;
  std::vector<SimAlgo> algo_subs;
  SimTaskQueue task_queue;
};

}  // namespace ngh::sim