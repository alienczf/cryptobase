#pragma once

#include <chrono>
#include <cinttypes>
#include <cstring>
#include <ctime>
#include <functional>
#include <iomanip>

#include "boost/io/ios_state.hpp"

namespace {
// strlen isn't constexpr. this is
size_t constexpr length(const char* str) {
  return *str ? 1 + length(str + 1) : 0;
}
// consider a sample output time to determine how big our buffer should be
static constexpr size_t kDotLen = 1;
static constexpr size_t kSpaceLen = 1;
static constexpr auto kDateLen = length("2020-01-01");  // %F
static constexpr auto kTimeLen = length("12:13:14");    // %T
static constexpr auto kMsecLen = length("123456");      // microseconds
static constexpr auto kTzLen = length("+0200");         // relative to UTC
static constexpr auto kRequiredLen =
    kDateLen + kSpaceLen + kTimeLen + kDotLen + kMsecLen;

static const char* kFormatStr = "%F %T.";  // time format
static constexpr size_t kSlackLen = 10;    // should be totally unnecessary.
}  // namespace

namespace std::chrono {

template <class Ch, class Tr>
std::basic_ostream<Ch, Tr>& operator<<(std::basic_ostream<Ch, Tr>& out,
                                       system_clock::time_point const& dt) {
  auto tp = std::chrono::system_clock::to_time_t(dt);
  struct tm t {};
  t.tm_isdst = -1;
  ::localtime_r(&tp, &t);
  char buf[::kRequiredLen + ::kSlackLen] = {};
  strftime(buf, sizeof(buf), ::kFormatStr, &t);
  auto micros = std::chrono::time_point_cast<std::chrono::microseconds>(dt) -
                std::chrono::time_point_cast<std::chrono::seconds>(dt);

  boost::io::ios_all_saver ias(out);
  out << buf << std::setfill('0') << std::setw(6) << micros.count();
  return out;
}

template <class Ch, class Tr>
std::basic_istream<Ch, Tr>& operator>>(std::basic_istream<Ch, Tr>& in,
                                       system_clock::time_point& dt) {
  struct tm t {};
  char buf[::kRequiredLen + ::kSlackLen] = {};
  in.read(buf, ::kRequiredLen);
  const auto msecs = std::chrono::microseconds(
      std::stoi(std::string(buf + ::kRequiredLen - ::kMsecLen, 6)));
  strptime(buf, ::kFormatStr, &t);
  // -1 will let the system determine dst automatically
  t.tm_isdst = -1;
  const time_t tm = ::mktime(&t);
  dt = std::chrono::system_clock::from_time_t(tm) + msecs;
  if (in && ('+' == in.peek() || '-' == in.peek())) {
    const bool plus('+' == in.peek());
    char tzBuf[::kTzLen] = {};
    in.read(tzBuf, ::kTzLen);
    struct tm tz {};
    strptime(tzBuf + 1, "%H%M", &tz);
    const auto offset((tz.tm_hour * 60 + tz.tm_min) * 60 * (plus ? 1 : -1));
    dt += std::chrono::seconds(offset + timezone);
  }

  return in;
};

// Explicit instantiation
template std::ostream& operator<<(std::ostream& out,
                                  system_clock::time_point const& dt);

template std::istream& operator>>(std::istream& in,
                                  system_clock::time_point& dt);

}  // namespace std::chrono