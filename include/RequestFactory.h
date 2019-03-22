#pragma once

#include "PluginImpl.h"
#include "RequestBase.h"
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <spine/HTTP.h>
#include <xercesc/dom/DOMDocument.hpp>
#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Factory class for creating apropriate WFS request objects from
 *          KVP and XML format WFS requests
 */
class RequestFactory
{
 public:
  typedef boost::function2<boost::shared_ptr<RequestBase>,
                           const std::string&,
                           const SmartMet::Spine::HTTP::Request&>
      parse_kvp_t;

  typedef boost::function3<boost::shared_ptr<RequestBase>,
                           const std::string&,
                           const xercesc::DOMDocument&,
                           const xercesc::DOMElement&>
      parse_xml_t;

 public:
  RequestFactory(PluginImpl& plugin_data);

  virtual ~RequestFactory();

  /**
   *   @brief Registers factory methods for creating WFS request objects
   *
   *   @param name the name of the request
   *   @param feature_id ID to register for use for GetCapabilities response
   *   @param create_from_kvp factory method for creating the request object from
   *          HTTP format KVP request. KVP is not supported if empty boost::function1
   *          provided.
   *   @param create_from_xml factory method for creating the request object from
   *          HTTP format XML request. XML is not supported if empty boost::function1
   *          provided.
   */
  RequestFactory& register_request_type(const std::string& name,
                                        const std::string& feature_id,
                                        parse_kvp_t create_from_kvp,
                                        parse_xml_t create_from_xml);

  /**
   *   @brief Registers name of unimplemented WFS request
   */
  RequestFactory& register_unimplemented_request_type(const std::string& name);

  /**
   *   @brief Creates WFS request object from KVP request
   *
   *   This method is for
   *   - HTTP GET when request parameters are provided inside request URL
   *   - HTTP POST with content type application/x-www-form-urlencoded when request
   *     parameters are provided in HTTP request body.
   */
  boost::shared_ptr<RequestBase> parse_kvp(
      const std::string& language, const SmartMet::Spine::HTTP::Request& http_request) const;

  /**
   *   @brief Creates WFS request object from XML format request
   *
   *   Note: SOAP requests are not yet supported
   */
  boost::shared_ptr<RequestBase> parse_xml(const std::string& language,
                                           const xercesc::DOMDocument& document) const;

  /**
   *   @brief Check whether provided request name is valid (although possibly unimplemented)
   *
   *   Warning: check is case sensitive in this case
   */
  inline bool check_request_name(const std::string& name) const
  {
    return request_names.count(name) > 0;
  }

 private:
  struct TypeRec
  {
    parse_kvp_t kvp_parser;
    parse_xml_t xml_parser;
  };

  const TypeRec& get_type_rec(const std::string& name) const;

  std::set<std::string> request_names;
  std::map<std::string, TypeRec> type_map;
  std::set<std::string> unimplemented_requests;

  PluginImpl& plugin_data;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
