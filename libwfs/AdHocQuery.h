#pragma once

#include "QueryBase.h"
#include "StoredQuery.h"
#include "StoredQueryHandlerBase.h"
#include "StoredQueryMap.h"
#include "TypeNameStoredQueryMap.h"
#include "XPathSnapshot.h"
#include <boost/shared_ptr.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class StoredQueryMap;
class StoredQueryHandlerBase;
class XPathSnapshot;

/**
 *   @brief Represents parsed ad hoc query request
 */
class AdHocQuery : public StoredQuery
{
 protected:
  AdHocQuery();

 public:
  virtual ~AdHocQuery();

  virtual QueryType get_type() const;

  /**
   *   @brief Static method for reading stored query from KVP format request
   *
   *   @param language The language code
   *   @param spp standard presentation parameters
   *   @param http_request The HTTP request with the query
   *   @param sq_map stored query map object
   *   @param typename_storedquery_map typename - stored queries map object
   *   @param queries Pointers to created query objects.
   *
   *   Throws SmartMet::Plugin::WFS::WfsException in case of an error. Usually error
   *   code is expected to be SmartMet::Plugin::WFS::WfsException::OPERATION_PARSING_FAILED.
   */
  static void create_from_kvp(const std::string& language,
                              const StandardPresentationParameters& spp,
                              const SmartMet::Spine::HTTP::Request& http_request,
                              const StoredQueryMap& sq_map,
                              const TypeNameStoredQueryMap& typename_storedquery_map,
                              std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Static method for reading ad hoc query from XML format request
   *
   *   @param language The language code.
   *   @param spp Standard presentation parameters.
   *   @param query_root Xerces-C DOM root element of the query.
   *   @param stored_query_map Stored query map object.
   *   @param typename_storedquery_map typename - stored queries map object
   *   @param queries Pointers to created query objects.
   *
   *   Throws SmartMet::Plugin::WFS::WfsException in case of an error. Usually error
   *   code is expected to be SmartMet::Plugin::WFS::WfsException::OPERATION_PARSING_FAILED.
   */

  static void create_from_xml(const std::string& language,
                              const StandardPresentationParameters& spp,
                              const xercesc::DOMElement& query_root,
                              const StoredQueryMap& stored_query_map,
                              const TypeNameStoredQueryMap& typename_storedquery_map,
                              std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Extract typenams from XML request
   *
   *   @param query_root Root DOM element of ad hoc query.
   *   @param typenames typenames in query
   *
   *   Element name wfs:Query without attribute typeNames causes it however
   *   to throw SmartMet::Plugin::WFS::WfsException instead of returning false.
   */
  static void get_xml_typenames(const xercesc::DOMElement& query_root,
                                std::vector<std::string>& typenames);

  /**
   *   @brief Starts XML element parsing for extracting stored query parameters and filtering
   *          rules.
   *
   *   @param query_root Root DOM element of ad hoc query.
   *   @param handler Handler for used stored query.
   *   @param element_tree List of query XML elements.
   *   @param queries Pointers to created query objects.
   *
   */
  static void extract_xml_parameters(const xercesc::DOMElement& query_root,
                                     boost::shared_ptr<const StoredQueryHandlerBase> handler,
                                     std::vector<std::string>& element_tree,
                                     std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Performs XPath filtering according to filtering expression of given query.
   *
   *   @param query Query which contains filtering expression.
   *   @param xps Query response.
   *
   */
  static void filter(boost::shared_ptr<QueryBase> query,
                     SmartMet::Plugin::WFS::Xml::XPathSnapshot& xps);

  /**
   *   @brief Do we have filter expression at all (Do we need to call filter-method).
   *
   *
   */
  inline bool has_filter() const { return not filter_expression.empty(); }
  /**
   *   @brief get filter expression.
   *
   *
   */
  inline const std::string& get_filter_expression() const { return filter_expression; }

 private:
  std::string filter_expression;

  /**
   *   @brief Static method for reading FILTER stored query from KVP format request
   *
   *   @param filter url-encoded xml-fragment containing filter definition
   *   @param filter_language optional parameter containing filter language, defaults to
   * "urn:ogc:def:query Language:OGC-FES:Filter"
   *   @param language The language code
   *   @param stored_query_ids query id in stored query map
   *   @param spp standard presentation parameters
   *   @param stored_query_map stored query map object
   *   @param queries Pointers to created query objects.
   *
   *   Throws SmartMet::Plugin::WFS::WfsException in case of an error. Usually error
   *   code is expected to be SmartMet::Plugin::WFS::WfsException::OPERATION_PARSING_FAILED.
   */
  static void create_filter_query_from_kvp(const std::string& filter,
                                           const boost::optional<std::string>& filter_language,
                                           const std::string& language,
                                           const std::vector<std::string>& stored_query_ids,
                                           const StandardPresentationParameters& spp,
                                           const StoredQueryMap& stored_query_map,
                                           std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Static method for reading BBOX stored query from KVP format request
   *
   *   @param bbox_string bounding box
   *   @param language The language code
   *   @param stored_query_ids query id in stored query map
   *   @param spp standard presentation parameters
   *   @param stored_query_map stored query map object
   *   @param queries Pointers to created query objects.
   *
   *   Throws SmartMet::Plugin::WFS::WfsException in case of an error. Usually error
   *   code is expected to be SmartMet::Plugin::WFS::WfsException::OPERATION_PARSING_FAILED.
   */
  static void create_bbox_query_from_kvp(const std::string& bbox_string,
                                         const std::string& language,
                                         const std::vector<std::string>& stored_query_ids,
                                         const StandardPresentationParameters& spp,
                                         const StoredQueryMap& stored_query_map,
                                         std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Adds projection clause to the given queries.
   *
   *   @param query_root Root element of ad hoc query.
   *   @param queries Pointers to created query objects.
   *
   */
  static void add_projection_clause_from_xml(const xercesc::DOMElement& query_root,
                                             std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Adds projection clause to the given queries.
   *
   *   @param property_names Property names read from GET/POST request.
   *   @param queries Pointers to created query objects.
   *
   */
  static void add_projection_clause(const std::vector<std::string>& property_names,
                                    std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Replaces aliases with type names in the given queries.
   *
   *   @param query_root Root element of ad hoc query.
   *   @param queries Pointers to query objects.
   *
   */
  static void replace_aliases_from_xml(const xercesc::DOMElement& query_root,
                                       std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Replaces aliases with type names in the given queries.
   *
   *   @param aliases Aliases read from GET/POST request.
   *   @param type_names Type names read from GET/POST request.
   *   @param queries Pointers to query objects.
   *
   */
  static void replace_aliases(const std::vector<std::string>& aliases,
                              const std::vector<std::string>& type_names,
                              std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Creates a new ad hoc query object to query list.
   *
   *   @param handler Handler for used stored query.
   *   @param queries Pointers to created query objects.
   *
   */
  static void create_query(boost::shared_ptr<const StoredQueryHandlerBase> handler,
                           std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Parses query XML elements for extracting stored query parameters and filtering
   *          rules.
   *
   *   @param root_element Root DOM element which content is parsed.
   *   @param element_tree List of query XML elements.
   *   @param queries Pointers to created query objects.
   *   @param upper_level_and_query Query object for AND element.
   *   @param or_query_indexes List of first query object indexes of OR elements.
   *
   */
  static void extract_filter_elements(const xercesc::DOMElement& root_element,
                                      std::vector<std::string>& element_tree,
                                      std::vector<boost::shared_ptr<QueryBase>>& queries,
                                      AdHocQuery* upper_level_and_query,
                                      std::vector<unsigned int>& or_query_indexes);

  /**
   *   @brief Performs parsing actions when finding end of and-element.
   *
   *   @param queries Pointers to created query objects.
   *   @param or_query_indexes List of first query object indexes of OR elements.
   *   @param and_query_index Index of query containing collected and-element parameters/filtering
   * expressions.
   *   @param and_query Pointer to query containing collected and-element parameters/filtering
   * expressions.
   *
   */
  static void handle_end_of_and_element(std::vector<boost::shared_ptr<QueryBase>>& queries,
                                        std::vector<unsigned int>& or_query_indexes,
                                        unsigned int and_query_index,
                                        AdHocQuery* and_query);

  /**
   *   @brief Reads comparison definitions from XML query.
   *
   *   @param element Current element in XML query.
   *   @param element_name Name of current element.
   *   @param query Pointer to query.
   *
   */
  static void read_comparison_operation(const xercesc::DOMElement& element,
                                        const std::string& element_name,
                                        AdHocQuery* query);

  /**
   *   @brief Reads query parameters from XML query.
   *
   *   @param element Current element in XML query.
   *   @param element_tree List of query XML elements.
   *   @param element_name Name of current element.
   *   @param param_name_set Query parameter list.
   *   @param forbidden Query parameter list.
   *   @param query Pointer to query.
   *
   */
  static void read_query_parameter(const xercesc::DOMElement& element,
                                   const std::vector<std::string>& element_tree,
                                   const std::string& element_name,
                                   std::multiset<std::string>& param_name_set,
                                   std::set<std::string>& forbidden,
                                   AdHocQuery* query);

  /**
   *   @brief Copies query parameters.
   *
   *   @param src_query Query from which parameters are copied.
   *   @param target_query Query to which parameters are copied.
   *
   */
  static void copy_params(const AdHocQuery* src_query, AdHocQuery* target_query);

  /**
   *   @brief sets query parameters.
   *
   *   @param language The language code.
   *   @param query_id query id in stored query map
   *   @param spp Standard presentation parameters.
   *   @param queries Pointers to created query objects.
   *
   */
  static void process_parms(const std::string& language,
                            const std::string& query_id,
                            const StandardPresentationParameters& spp,
                            std::vector<boost::shared_ptr<QueryBase>>& queries);

  /**
   *   @brief Checks if the given query is used, that is there is some query parameters or
   *          filtering expressions defined.
   *
   *   @param query Pointer to query.
   *   @return Indicates whether query is used.
   *
   */
  static bool is_query_used(const AdHocQuery* query);

  /**
   *   @brief Find storedqueryids from typenames
   *
   *   @param typename_storedquery_map Type name - stored query id - map.
   *   @param type_names Typenames in query.
   *   @param stored_query_ids Resulting stored query ids..
   *
   */
  static void get_stored_query_ids_from_typenames(
      const TypeNameStoredQueryMap& typename_storedquery_map,
      const std::vector<std::string>& type_names,
      std::vector<std::string>& stored_query_ids);
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
