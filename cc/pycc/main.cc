#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "ngh/data/qsbinhandler.h"
#include "ngh/mkt/ftxhandler.h"
#include "ngh/sim/Session.h"
#include "ngh/types/types.h"
#include "pybind11/pybind11.h"

PYBIND11_MAKE_OPAQUE(ngh::Refs);
PYBIND11_MAKE_OPAQUE(ngh::PriceBook);
PYBIND11_MAKE_OPAQUE(ngh::Balances);
PYBIND11_MAKE_OPAQUE(ngh::OrderBook);

namespace pycc {

PYBIND11_MODULE(pycc, m) {
  auto m_ngh = m.def_submodule("ngh");
  pybind11::class_<ngh::Order>(m_ngh, "Order")
      .def_readonly("id", &ngh::Order::id)
      .def_readonly("type", &ngh::Order::type)
      .def_readonly("side", &ngh::Order::side)
      .def_readonly("price", &ngh::Order::price)
      .def_readonly("size", &ngh::Order::size)
      .def_readonly("status", &ngh::Order::status)
      .def_readonly("filledSize", &ngh::Order::filledSize)
      .def_readonly("remainingSize", &ngh::Order::remainingSize)
      .def_readonly("reduceOnly", &ngh::Order::reduceOnly)
      .def_readonly("postOnly", &ngh::Order::postOnly)
      .def_readonly("ioc", &ngh::Order::ioc);
  pybind11::bind_map<ngh::PriceBook>(m_ngh, "PriceBook",
                                     pybind11::module_local(true));
  pybind11::bind_map<ngh::OrderBook>(m_ngh, "OrderBook",
                                     pybind11::module_local(true));
  pybind11::bind_map<ngh::Balances>(m_ngh, "Balances",
                                    pybind11::module_local(true));
  pybind11::class_<ngh::Ref>(m_ngh, "Ref")
      .def(pybind11::init<>())
      .def_readwrite("price_exp", &ngh::Ref::price_exp)
      .def_readwrite("qty_exp", &ngh::Ref::qty_exp)
      .def_readwrite("hedge_ratio", &ngh::Ref::hedge_ratio)
      .def_readwrite("price_ratio", &ngh::Ref::price_ratio)
      .def_readwrite("px_offset", &ngh::Ref::px_offset)
      .def_readwrite("mark_px", &ngh::Ref::mark_px);
  pybind11::bind_vector<ngh::Refs>(m_ngh, "Refs", pybind11::module_local(true));

  auto m_mkt = m_ngh.def_submodule("mkt");
  pybind11::class_<ngh::mkt::L2StateTracker>(m_mkt, "L2StateTracker")
      .def_readonly("lastTs", &ngh::mkt::L2StateTracker::lastTs)
      .def("getBids", &ngh::mkt::L2StateTracker::getBids,
           pybind11::return_value_policy::reference_internal)
      .def("getAsks", &ngh::mkt::L2StateTracker::getAsks,
           pybind11::return_value_policy::reference_internal)
      .def_readonly("lastTradeTs", &ngh::mkt::L2StateTracker::lastTradeTs)
      .def_readonly("lastTradePx", &ngh::mkt::L2StateTracker::lastTradePx)
      .def_readonly("lastTradeQty", &ngh::mkt::L2StateTracker::lastTradeQty)
      .def_readonly("lastTradeIsLiquidation",
                    &ngh::mkt::L2StateTracker::lastTradeIsLiquidation)
      .def_readwrite("orders", &ngh::mkt::L2StateTracker::orders);

  auto m_data = m_ngh.def_submodule("data");
  m_data.def("LoadQsBinFile", &ngh::data::LoadQsBinFile);

  auto m_sim = m_ngh.def_submodule("sim");
  pybind11::class_<ngh::sim::SimSession>(m_sim, "SimSession")
      .def(pybind11::init<>())
      .def("LoadQsBinFiles", &ngh::sim::SimSession::LoadQsBinFiles)
      .def("Reset", &ngh::sim::SimSession::Reset)
      .def("Setup", &ngh::sim::SimSession::Setup)
      .def("AddAlgo", &ngh::sim::SimSession::AddAlgo)
      .def("Run", &ngh::sim::SimSession::Run);
}

}  // namespace pycc
