#pragma once
#include <ngh/data/qsbinhandler.h>
#include <ngh/sim/Exchange.h>
#include <ngh/sim/Messaging.h>
#include <ngh/sim/ReplayAlgo.h>
#include <ngh/sim/SimTaskQueue.h>

#include <map>

namespace ngh::sim {

class SimSession {
 public:
  SimSession() {}
  void AddExchange(const data::alc::EXCHID exch, const data::alc::SYMID symbol,
                   const std::vector<Packet>& pkts) {
    if (!ts_pubs
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(exch, symbol),
                      std::forward_as_tuple(task_queue, pkts))
             .second) {
      LOGWRN("data already loaded for %d %d. cowardly refusing to reload",
             symbol, exch);
    }
  }

  void AddDataPublisher(const data::alc::EXCHID exch,
                        const data::alc::SYMID symbol,
                        const std::vector<Packet>& pkts) {
    if (!qs_pubs
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(exch, symbol),
                      std::forward_as_tuple(task_queue, pkts))
             .second) {
      LOGWRN("data already loaded for %d %d. cowardly refusing to reload",
             symbol, exch);
    }
  }

  void LoadQsBinFiles(std::vector<std::string> files, data::alc::EXCHID exch,
                      data::alc::SYMID symbol) {
    std::vector<Packet> pkts{};
    for (const auto& fn : files) {
      for (auto& pkt : LoadBin(fn)) {
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
    for (auto& [_, pub] : qs_pubs) {
      pub.Setup();
    }
    for (auto& [_, pub] : ts_pubs) {
      pub.Setup();
    }
    for (auto& algo : algo_subs) {
      algo.Setup();
    }
  }

  void AddAlgo(data::alc::EXCHID exch, data::alc::SYMID symbol,
               data::UnixTimeMicro latency, Reqs& reqs) {
    auto tit = ts_pubs.find({exch, symbol});
    auto qit = qs_pubs.find({exch, symbol});
    if (tit == ts_pubs.end()) {
      LOGWRN("no ts_pubs for %d %d", symbol, exch);
      return;
    }
    if (qit == qs_pubs.end()) {
      LOGWRN("no qs_pubs for %d %d", symbol, exch);
    }

    // TODO(ANY): fetch latency config map
    algo_subs.push_back({task_queue, tit->second, reqs, latency});
    auto& algo = *algo_subs.rbegin();
    tit->second.Subscribe(latency, algo);
    qit->second.Subscribe(latency, algo);
  }

  void Run() { task_queue.RunUntilDone(); }

  std::map<std::pair<data::alc::EXCHID, data::alc::SYMID>, NoImpactExchange>
      ts_pubs;
  std::map<std::pair<data::alc::EXCHID, data::alc::SYMID>, SimQsPublisher>
      qs_pubs;
  std::vector<ReplayAlgo> algo_subs;
  SimTaskQueue task_queue;
};

}  // namespace ngh::sim