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

static std::stringstream LoadBin(const std::string& filename) {
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

static std::vector<Packet> LoadPacket(std::stringstream&& decompressed) {
  std::vector<Packet> pkts{};
  size_t i = 0;
  while (!decompressed.eof()) {
    pkts.push_back(Packet::MakePacket(decompressed));
    ++i;
  }
  std::cout << "read packets:" << i << std::endl;
  return pkts;
}

static void LoadQsBinFile(const std::string& fn) {
  mkt::PktHandler h{};
  for (auto& pkt : LoadPacket(LoadBin(fn))) {
    h.OnPacket(pkt);
  }
}

}  // namespace ngh::data