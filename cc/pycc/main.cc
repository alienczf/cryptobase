#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "ngh/data/qsbinhandler.h"
#include "ngh/log.h"
#include "ngh/mkt/ftxhandler.h"
#include "ngh/sim/Messaging.h"
#include "ngh/sim/Session.h"
#include "ngh/types/types.h"
#include "pybind11/pybind11.h"

PYBIND11_MAKE_OPAQUE(ngh::Refs);
PYBIND11_MAKE_OPAQUE(ngh::PriceBook);
PYBIND11_MAKE_OPAQUE(ngh::Balances);
PYBIND11_MAKE_OPAQUE(ngh::OrderBook);
PYBIND11_MAKE_OPAQUE(ngh::sim::Reqs);
PYBIND11_MAKE_OPAQUE(ngh::data::alc::Pkts);
PYBIND11_MAKE_OPAQUE(ngh::data::alc::Syms);

namespace pycc {

PYBIND11_MODULE(pycc, m) {
  auto m_ngh = m.def_submodule("ngh");
  {
    using namespace ngh;
    m_ngh.def("SetupLogger", &log::SetupLogger);
    pybind11::class_<Order>(m_ngh, "Order")
        .def_readonly("id", &Order::id)
        .def_readonly("type", &Order::type)
        .def_readonly("side", &Order::side)
        .def_readonly("price", &Order::price)
        .def_readonly("size", &Order::size)
        .def_readonly("status", &Order::status)
        .def_readonly("filledSize", &Order::filledSize)
        .def_readonly("remainingSize", &Order::remainingSize)
        .def_readonly("reduceOnly", &Order::reduceOnly)
        .def_readonly("postOnly", &Order::postOnly)
        .def_readonly("ioc", &Order::ioc);
    pybind11::bind_map<PriceBook>(m_ngh, "PriceBook",
                                  pybind11::module_local(true));
    pybind11::bind_map<OrderBook>(m_ngh, "OrderBook",
                                  pybind11::module_local(true));
    pybind11::bind_map<Balances>(m_ngh, "Balances",
                                 pybind11::module_local(true));
    pybind11::class_<Ref>(m_ngh, "Ref")
        .def(pybind11::init<>())
        .def_readwrite("price_exp", &Ref::price_exp)
        .def_readwrite("qty_exp", &Ref::qty_exp)
        .def_readwrite("hedge_ratio", &Ref::hedge_ratio)
        .def_readwrite("price_ratio", &Ref::price_ratio)
        .def_readwrite("px_offset", &Ref::px_offset)
        .def_readwrite("mark_px", &Ref::mark_px);
    pybind11::bind_vector<Refs>(m_ngh, "Refs", pybind11::module_local(true));

    auto m_mkt = m_ngh.def_submodule("mkt");
    {
      using namespace mkt;
      pybind11::class_<L2StateTracker>(m_mkt, "L2StateTracker")
          .def_readonly("lastTs", &L2StateTracker::lastTs)
          .def("getBids", &L2StateTracker::getBids,
               pybind11::return_value_policy::reference_internal)
          .def("getAsks", &L2StateTracker::getAsks,
               pybind11::return_value_policy::reference_internal)
          .def_readonly("lastTradeTs", &L2StateTracker::lastTradeTs)
          .def_readonly("lastTradePx", &L2StateTracker::lastTradePx)
          .def_readonly("lastTradeQty", &L2StateTracker::lastTradeQty)
          .def_readonly("lastTradeIsLiquidation",
                        &L2StateTracker::lastTradeIsLiquidation)
          .def_readwrite("orders", &L2StateTracker::orders);
    }
    auto m_data = m_ngh.def_submodule("data");
    {
      using namespace data;
      m_data.def("LoadQsBinFile", &LoadQsBinFile);
      m_data.def("LoadBin", &LoadBin);
      auto m_alc = m_data.def_submodule("alc");
      {
        using namespace alc;
        pybind11::enum_<MakerSide>(m_alc, "MakerSide")
            .value("X", ngh::data::alc::MakerSide::X)
            .value("B", ngh::data::alc::MakerSide::B)
            .value("S", ngh::data::alc::MakerSide::S)
            .export_values();
        pybind11::class_<Packet>(m_alc, "SimReqSubmit")
            .def_readwrite("snapshot", &Packet::snapshot)
            .def_readwrite("inferred", &Packet::inferred)
            .def_readwrite("filtered", &Packet::filtered)
            .def_readwrite("qs_latency", &Packet::qs_latency)
            .def_readwrite("seq_num", &Packet::seq_num)
            .def_readwrite("md_id", &Packet::md_id)
            .def_readwrite("qs_send_time", &Packet::qs_send_time)
            .def_readwrite("exch_time", &Packet::exch_time)
            .def_readwrite("ts", &Packet::ts);
        pybind11::bind_vector<Pkts>(m_alc, "Pkts",
                                    pybind11::module_local(true));
        pybind11::bind_vector<Syms>(m_alc, "Syms",
                                    pybind11::module_local(true));
      }
    }
    auto m_sim = m_ngh.def_submodule("sim");
    {
      using namespace sim;
      pybind11::enum_<TIF>(m_sim, "TIF")
          .value("GTC", ngh::sim::GTC)
          .value("FAK", ngh::sim::FAK)
          .value("POST", ngh::sim::POST)
          .export_values();
      pybind11::class_<SimReqSubmit> c_submit(m_sim, "SimReqSubmit");
      c_submit.def(pybind11::init<>())
          .def_readwrite("tif", &SimReqSubmit::tif)
          .def_readwrite("oid", &SimReqSubmit::oid)
          .def_readwrite("side", &SimReqSubmit::side)
          .def_readwrite("prc", &SimReqSubmit::prc)
          .def_readwrite("qty", &SimReqSubmit::qty);
      pybind11::class_<SimReqCancel>(m_sim, "SimReqCancel")
          .def(pybind11::init<>())
          .def_readwrite("oid", &SimReqCancel::oid)
          .def_readwrite("side", &SimReqCancel::side)
          .def_readwrite("prc", &SimReqCancel::prc);
      pybind11::class_<SimReq> c_req(m_sim, "SimReq");
      c_req.def(pybind11::init<>())
          .def_readwrite("type", &SimReq::type)
          .def_readwrite("ts", &SimReq::ts)
          .def_readwrite("submit", &SimReq::submit)
          .def_readwrite("cancel", &SimReq::cancel);
      pybind11::enum_<SimReq::Type>(c_req, "Type")
          .value("SUBMIT", ngh::sim::SimReq::SUBMIT)
          .value("CANCEL", ngh::sim::SimReq::CANCEL)
          .export_values();
      pybind11::bind_vector<Reqs>(m_sim, "Reqs", pybind11::module_local(true));
      pybind11::class_<SimSession>(m_sim, "SimSession")
          .def(pybind11::init<>())
          .def("LoadQsBinFiles", &SimSession::LoadQsBinFiles)
          .def("AddExchange", &SimSession::AddExchange)
          .def("AddDataPublisher", &SimSession::AddDataPublisher)
          .def("Reset", &SimSession::Reset)
          .def("Setup", &SimSession::Setup)
          .def("AddAlgo", &SimSession::AddAlgo)
          .def("Run", &SimSession::Run);
    }
  }
}

}  // namespace pycc
