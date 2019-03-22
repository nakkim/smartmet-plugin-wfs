#pragma once

#include <boost/variant.hpp>
#include <ctpp2/CDT.hpp>
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <exception>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class PluginImpl;
class StoredQuery;
// class WfsException;

class ErrorResponseGenerator
{
 public:
  enum processing_phase_t
  {
    REQ_PARSING,
    REQ_PROCESSING
  };

  struct ErrorResponse
  {
    SmartMet::Spine::HTTP::Status status;
    std::string response;
    std::string log_message;
    std::string wfs_err_code;
  };

 public:
  ErrorResponseGenerator(PluginImpl& plugin_data);

  virtual ~ErrorResponseGenerator();

  /**
   *   @brief This procedure must be called in exception context (catch block)
   */
  ErrorResponse create_error_response(
      processing_phase_t phase,
      boost::variant<const SmartMet::Spine::HTTP::Request&, StoredQuery&> query_info);

 private:
  CTPP::CDT handle_wfs_exception(SmartMet::Spine::Exception& err);

  CTPP::CDT handle_std_exception(const std::exception& err, processing_phase_t phase);

  CTPP::CDT handle_unknown_exception(processing_phase_t phase);

  std::string get_wfs_err_code(processing_phase_t phase);

  void add_query_info(
      CTPP::CDT& hash,
      boost::variant<const SmartMet::Spine::HTTP::Request&, StoredQuery&> query_info);

  void add_http_request_info(CTPP::CDT& hash, const SmartMet::Spine::HTTP::Request& request);

  void add_stored_query_info(CTPP::CDT& hash, const StoredQuery& query);

  std::string format_message(CTPP::CDT& hash);

  std::string format_log_message(CTPP::CDT& hash);

 private:
  PluginImpl& plugin_data;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
