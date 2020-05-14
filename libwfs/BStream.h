#pragma once

#include <boost/shared_array.hpp>
#include <spine/Value.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class OBStream
{
 public:
  OBStream();
  virtual ~OBStream();
  std::string raw_data() const;
  operator std::string() const;
  void put_bit(bool val);
  void put_bits(unsigned value, int num);
  void put_int(int64_t value);
  void put_unsigned(uint64_t value);
  void put_double(double value);
  void put_char(char c);
  void put_string(const std::string& text);
  void put_ptime(const boost::posix_time::ptime& tm);
  void put_value(const SmartMet::Spine::Value& value);
  void put_value_map(const std::multimap<std::string, SmartMet::Spine::Value>& data);

 private:
  std::size_t reserved;
  boost::shared_array<uint8_t> data;
  std::size_t ind;
  int num_bits;
};

class IBStream
{
 public:
  IBStream(const uint8_t* data, std::size_t length);
  virtual ~IBStream();
  unsigned get_bit();
  unsigned get_bits(int num);
  uint64_t get_unsigned();
  int64_t get_int();
  double get_double();
  char get_char();
  std::string get_string();
  boost::posix_time::ptime get_ptime();
  SmartMet::Spine::Value get_value();
  std::multimap<std::string, SmartMet::Spine::Value> get_value_map();

 private:
  std::size_t length;
  std::size_t pos;
  boost::shared_array<uint8_t> data;
  int bit_pos;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
