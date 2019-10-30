#pragma once

#include "QueryBase.h"
#include "RequestParameterMap.h"
#include "StandardPresentationParameters.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryMap.h"
#include "XmlParameterExtractor.h"
#include <boost/shared_ptr.hpp>
#include <spine/Value.h>
#include <xercesc/dom/DOMElement.hpp>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredQueryConfig;
class StoredQueryMap;
class StoredQueryHandlerBase;

/**
 *   @brief Represents parsed stored query request with its ID and parameters
 */
class StoredQuery : public QueryBase
{
 protected:
  StoredQuery();

 public:
  virtual ~StoredQuery();

  /**
   *   @brief Static method for reading stored query from KVP format request
   *
   *   @param language The language code
   *   @param spp standard presentation parameters
   *   @param http_request The HTTP request with the query
   *   @param sq_map stored query map object
   *   @return The shared pointer to created query object
   *
   *   Throws SmartMet::Plugin::WFS::WfsException in case of an error. Usually error
   *   code is expected to be SmartMet::Plugin::WFS::WfsException::OPERATION_PARSING_FAILED.
   */
  static boost::shared_ptr<StoredQuery> create_from_kvp(
      const std::string& language,
      const StandardPresentationParameters& spp,
      const SmartMet::Spine::HTTP::Request& http_request,
      const StoredQueryMap& sq_map);

  /**
   *   @brief Static method for reading stored query from XML format request
   *
   *   @param language The language code
   *   @param spp standard presentation parameters
   *   @param element Xerces-C DOM root element of the query
   *   @param sq_map stored query map object
   *   @return The shared pointer to created query object
   *
   *   Throws SmartMet::Plugin::WFS::WfsException in case of an error. Usually error
   *   code is expected to be SmartMet::Plugin::WFS::WfsException::OPERATION_PARSING_FAILED.
   */
  static boost::shared_ptr<StoredQuery> create_from_xml(const std::string& language,
                                                        const StandardPresentationParameters& spp,
                                                        const xercesc::DOMElement& element,
                                                        const StoredQueryMap& sq_map);

  static boost::shared_ptr<StoredQuery> create_from_feature_id(const std::string& feature_id,
                                                               const StoredQueryMap& sq_map,
                                                               const StoredQuery& orig_query);

  virtual QueryType get_type() const;

  const std::string& get_stored_query_id() const { return id; }
  virtual std::string get_cache_key() const;

  bool get_use_debug_format() const { return debug_format; }
  virtual void execute(std::ostream& output, const std::string& language, const boost::optional<std::string>& hostname) const;

  const SmartMet::Spine::Value& get_param(const std::string& name) const;

  std::vector<SmartMet::Spine::Value> get_param_values(const std::string& name) const;

  /**
   *   @brief Tries to get stored query request ID from KVP request
   *
   *   @param http_request The incoming HTTP request
   *   @param stored_query_id The variable where to store retrieved request ID
   *           in the case of success. Not changed if the ID is not retrieved
   *           from the request.
   *   @return Indicates whether stored request ID has been extracted.
   *           Not necessarily indicates an error as this method can be used
   *           to detect that the incoming request is stored query request.
   */
  static bool get_kvp_stored_query_id(const SmartMet::Spine::HTTP::Request& http_request,
                                      std::string* stored_query_id);

  /**
   *   @brief Tries to get stored query request ID from XML request
   *
   *   @param query_root root DOM element of stored query
   *   @param stored_query_id The variable where to store retrieved request ID
   *           in the case of success. Not changed if the ID is not retrieved
   *           from the request.
   *   @return Indicates whether stored request ID has been extracted.
   *           Not necessarily indicates an error as this method can be used
   *           to detect that the incoming request is stored query request.
   *
   *   Element name wfs:StoredQuery without attribute id causes it however
   *   to throw SmartMet::Plugin::WFS::WfsException instead of returning false.
   */
  static bool get_xml_stored_query_id(const xercesc::DOMElement& query_root,
                                      std::string* stored_query_id);

  /**
   *   @brief Extracts KVP format stored query parameters according to
   *          found query configuration and actual parameters provided
   *          in KVP request
   *
   *   It is static only to make testing it separately more easier.
   */
  static void extract_kvp_parameters(const SmartMet::Spine::HTTP::Request& request,
                                     const StoredQueryConfig& config,
                                     StoredQuery& query);

  /**
   *   @brief Extracts XML format stored query parameters according to
   *          found query configuration and actual parameters provided
   *          in KVP request
   *
   *   It is static only to make testing it separately more easier.
   *
   *   Procedure should also work for SOAP format XML requests
   *   as query content itself in the XML document is the same only
   *   place different.
   */
  static void extract_xml_parameters(const xercesc::DOMElement& query_root,
                                     const StoredQueryConfig& config,
                                     StoredQuery& query);

  inline const RequestParameterMap& get_param_map() const { return *params; }
  void dump_query_info(std::ostream& output) const;

 protected:
  std::string id;
  std::string cache_key;
  std::string language;
  boost::shared_ptr<RequestParameterMap> params;
  boost::shared_ptr<RequestParameterMap> orig_params;
  std::vector<std::string> skipped_params;
  boost::shared_ptr<const SmartMet::Plugin::WFS::StoredQueryHandlerBase> handler;
  bool debug_format;

  static SmartMet::Plugin::WFS::Xml::ParameterExtractor param_extractor;
};

std::ostream& operator<<(std::ostream& ost,
                         const std::map<std::string, std::vector<SmartMet::Spine::Value> >& params);

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
