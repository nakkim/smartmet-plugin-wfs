#pragma once

#include <spine/HTTP.h>
#include <xercesc/dom/DOMElement.hpp>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   Represents XML attribute group wfs:StandardPresentationParameters
 */
class StandardPresentationParameters
{
 public:
  typedef unsigned spp_index_t;

  enum SPPResultType
  {
    SPP_RESULTS,
    SPP_HITS
  };

  static const char* DEFAULT_OUTPUT_FORMAT;
  static const char* DEBUG_OUTPUT_FORMAT;

 public:
  StandardPresentationParameters();
  virtual ~StandardPresentationParameters();

  void read_from_kvp(const SmartMet::Spine::HTTP::Request& http_request);
  void read_from_xml(const xercesc::DOMElement& element);

  inline bool get_have_counts() const { return have_counts; }
  inline spp_index_t get_start_index() const { return start_index; }
  inline spp_index_t get_count() const { return count; }
  inline std::string get_output_format() const { return output_format; }
  inline SPPResultType get_result_type() const { return result_type; }
  inline bool is_hits_only_request() const { return result_type == SPP_HITS; }

 private:
  void set_output_format(const std::string& str);
  void set_result_type(const std::string& str);

 private:
  bool have_counts;
  spp_index_t start_index;
  spp_index_t count;
  std::string output_format;
  SPPResultType result_type;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
