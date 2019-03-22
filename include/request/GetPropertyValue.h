#pragma once

#include "PluginImpl.h"
#include "QueryBase.h"
#include "RequestBase.h"
#include "StandardPresentationParameters.h"
#include "StoredQueryMap.h"
#include "XPathSnapshot.h"
#include <macgyver/TimedCache.h>
#include <xercesc/dom/DOMDocument.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
namespace Request
{
/**
 *   @brief Represents GetPropertyValue request
 */
class GetPropertyValue : public RequestBase
{
 private:
  GetPropertyValue(const std::string& language,
                   PluginImpl& plugin_data);

 public:
  virtual ~GetPropertyValue();

  virtual RequestType get_type() const;

  /**
   *   @brief Execute request and output result to stream
   *
   *   Throws SmartMet::Plugin::WFS::WfsException in case of an error.
   */
  virtual void execute(std::ostream& ost) const;

  /**
   *   @brief Create request from http GET request
   *
   *   @param language output language
   *   @param http_request parsed http get request
   *   @param plugin_data plugin data for spp, map etc.
   *   @param query_cache cached stored query responses.
   */
  static boost::shared_ptr<GetPropertyValue> create_from_kvp(
      const std::string& language,
      const SmartMet::Spine::HTTP::Request& http_request,
      PluginImpl& plugin_data);

  /**
   *   @brief Create request from http GET request
   *
   *   @param language output language
   *   @param document request parsed from http-post-request
   *   @param plugin_data plugin data for spp, map etc.
   *   @param query_cache cached stored query responses.
   */
  static boost::shared_ptr<GetPropertyValue> create_from_xml(const std::string& language,
                                                             const xercesc::DOMDocument& document,
                                                             PluginImpl& plugin_data);

  /**
   *   @brief Get response expiration time
   *
   *   @retval Smallest expiration time from queries, or default from plugin data, if no expiration
   * times found.
   */
  virtual int get_response_expires_seconds() const;

 private:
  /**
   *   @brief Get cached responses for stored queries
   *
   *   @retval True, if all queries have cached response, otherwise false.
   */
  bool get_cached_responses();

  /**
   *   @brief Get Start index and count from Standard Presentattion Parameters
   *
   *   @param max_members maximum number of members in output
   *   @param start_index index of first member from responses to go output
   */
  void initialize(boost::optional<int>& max_members, boost::optional<int>& start_index) const;

  /**
   *   @brief Add hits and members received etc. to final output
   *
   *   @param result result tree
   *   @param cumulative_num_returned number of members returned, i.e. included in result tree
   *   @param cumulative_num_matched number of hits in all responsens, i.e. those not included in
   * result tree are included in this number
   *   @param is_timestamp_set value "false", timestamp has not been already set, so set it here.
   *   @param is_schemalocation_set value "false", schema location has not been already set, so set
   * it here.
   */
  void finalize(boost::shared_ptr<xercesc::DOMDocument> result,
                const int cumulative_num_returned,
                const int cumulative_num_matched,
                const bool is_timestamp_set,
                const bool is_schemalocation_set) const;

  /**
   *   @brief Add collected responses to result tree
   *
   *   @param result result tree
   *   @param query_responses collected responses from queries
   *   @param cumulative_num_returned keep count of number of members returned, i.e. included in
   * result tree
   *   @param cumulative_num_matched keep count of number of hits in all responsens, i.e. those not
   * included in result tree are included in this number
   *   @param is_timestamp_set value changes to "true", when valid timestamp has been copied from
   * query response to result tree.
   *   @param is_schemalocation_set value changes to "true", when valid schema location has been
   * copied from query response to result tree.
   *
   *   Throws xercesc::DOMException or SmartMet::Plugin::WFS::Xml::XmlError in case of an error.
   */
  void add_responses(boost::shared_ptr<xercesc::DOMDocument> result,
                     const std::vector<std::string>& query_responses,
                     int& cumulative_num_returned,
                     int& cumulative_num_matched,
                     bool& is_timestamp_set,
                     bool& is_schemalocation_set) const;

  /**
   *   @brief Perform response filtering, if this is Ad Hoc-query and filter has been defined.
   * Otherwise simply pass thru extract_property-method.
   *
   *   @param result result tree
   *   @param response response from query
   *   @param cumulative_num_returned keep count of number of members returned, i.e. included in
   * result tree
   *   @param cumulative_num_matched keep count of number of hits in all responsens, i.e. those not
   * included in result tree are included in this number
   *   @param max_members maximum number of members still needed in output, i.e. this parameter
   * decreases as output progresses.
   *   @param start_index current index of first member from responses to go output, i.e. this
   * parameter decreases as output progresses.
   *   @param query is the target of filtering
   *   @param is_timestamp_set value changes to "true", when valid timestamp has been copied from
   * query response to result tree.
   *   @param is_schemalocation_set value changes to "true", when valid schema location has been
   * copied from query response to result tree.
   */
  void filter(boost::shared_ptr<xercesc::DOMDocument> result,
              const std::string& response,
              int& cumulative_num_returned,
              int& cumulative_num_matched,
              boost::optional<int>& max_members,
              boost::optional<int>& start_index,
              const boost::shared_ptr<QueryBase> query,
              bool& is_timestamp_set,
              bool& is_schemalocation_set) const;

  /**
   *   @brief Extract only desired property. (perform XPath-query to get only what is wanted)
   *
   *   @param result result tree
   *   @param response response from query, possibly already filtered.
   *   @param cumulative_num_returned keep count of number of members returned, i.e. included in
   * result tree
   *   @param cumulative_num_matched keep count of number of hits in all responsens, i.e. those not
   * included in result tree are included in this number
   *   @param max_members maximum number of members still needed in output, i.e. this parameter
   * decreases as output progresses.
   *   @param start_index current index of first member from responses to go output, i.e. this
   * parameter decreases as output progresses.
   *   @param is_timestamp_set value changes to "true", when valid timestamp has been copied from
   * query response to result tree.
   *   @param is_schemalocation_set value changes to "true", when valid schema location has been
   * copied from query response to result tree.
   */
  void extract_property(boost::shared_ptr<xercesc::DOMDocument> result,
                        const std::string& response,
                        int& cumulative_num_returned,
                        int& cumulative_num_matched,
                        boost::optional<int>& max_members,
                        boost::optional<int>& start_index,
                        bool& is_timestamp_set,
                        bool& is_schemalocation_set) const;

  /**
   *   @brief Actually append the filtered, extracted members to result tree and keep tab of
   * cumulatives
   *
   *   @param result result tree
   *   @param xpath_snapshot response parsed.
   *   @param cumulative_num_returned keep count of number of members returned, i.e. included in
   * result tree
   *   @param max_members maximum number of members still needed in output, i.e. this parameter
   * decreases as output progresses.
   *   @param start_index current index of first member from responses to go output, i.e. this
   * parameter decreases as output progresses.
   *   @param is_timestamp_set value changes to "true", when valid timestamp has been copied from
   * query response to result tree.
   *   @param is_schemalocation_set value changes to "true", when valid schema location has been
   * copied from query response to result tree.
   */
  void append_members(boost::shared_ptr<xercesc::DOMDocument> result,
                      Xml::XPathSnapshot& xpath_snapshot,
                      int& cumulative_num_returned,
                      boost::optional<int>& max_members,
                      boost::optional<int>& start_index,
                      bool& is_timestamp_set,
                      bool& is_schemalocation_set) const;

  /**
   *   @brief Calculate what to append in result tree, based on what was set in Standard
   * Presentation Parameters
   *
   *   @param max_members maximum number of members still needed in output, i.e. this parameter
   * decreases as output progresses.
   *   @param start_index current index of first member from responses to go output, i.e. this
   * parameter decreases as output progresses.
   *   @param num_members number of members in current response.
   *   @param first index of first member to include to result tree.
   *   @param last index of last member to include to result tree.
   */
  void calculate_loop_limits(const boost::optional<int>& max_members,
                             const boost::optional<int>& start_index,
                             const int num_members,
                             int& first,
                             int& last) const;

  /**
   *   @brief Copy schema location
   *
   *   @param source copy source, i.e. parsed response.
   *   @param destination destination, i.e. result tree.
   *   @retval True, if source had valid schema location attribute, and it was copied, otherwise
   * false.
   */
  bool copy_schema_location(const xercesc::DOMElement* source,
                            xercesc::DOMElement* destination) const;

  /**
   *   @brief Copy timestamp
   *
   *   @param source copy source, i.e. parsed response.
   *   @param destination destination, i.e. result tree.
   *   @retval True, if source had valid timestamp attribute, and it was copied, otherwise false.
   */
  bool copy_timestamp(const xercesc::DOMElement* source, xercesc::DOMElement* destination) const;
  /**
   *   @brief Collects responses of all queries from the GetValueProperty request as strings
   *
   *   @param query_responses A vector where to put the response strings
   *   @retval true at least one query succeeded
   *   @retval false none of queries succeeded
   */
  bool collect_query_responses(std::vector<std::string>& query_responses) const;

  /**
   *   @brief Substitutes apikey and pushes to query responses
   *
   *   @param query_responses A vector where to put the response strings
   *   @param response Response string
   */
  void add_query_responses(std::vector<std::string>& query_responses,
                           const std::string& response) const;

 private:
  std::vector<boost::shared_ptr<QueryBase> > queries;
  std::string xpath_string;
  StandardPresentationParameters spp;
  QueryResponseCache& query_cache;
  PluginImpl& plugin_data;
  bool fast;
};

}  // namespace Request
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
