#pragma once

#include "StandardPresentationParameters.h"
#include <ostream>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/**
 *   @brief Abstract base class for WFS queries (Query, StoredQuery)
 */
class QueryBase
{
 public:
  static const char* FMI_APIKEY_SUBST;
  static const char* FMI_APIKEY_PREFIX_SUBST;
  static const char* HOSTNAME_SUBST;
  static const char* PROTOCOL_SUBST;

 public:
  enum QueryType
  {
    QUERY,
    STORED_QUERY
  };

 public:
  QueryBase();

  virtual ~QueryBase();

  /**
   *   @brief Get type of the query
   */
  virtual QueryType get_type() const = 0;

  /**
   *   @brief Get key for use in cache
   */
  virtual std::string get_cache_key() const = 0;

  /**
   *   @brief Executes the query and writes the result to provided output stream.
   *
   *  Method is expected to throw an exception in a case of an error
   *  (SmartMet::Plugin::WFS::Exception preferred but not mandatory)
   */
  virtual void execute(std::ostream& output, const std::string& language, const boost::optional<std::string>& hostname) const = 0;

  /**
   *   @brief Cast to required query type (Query or StoredQuery) from
   *          the pointer to base class
   *
   *   Throws std::bad_cast when type cast fails
   */
  template <typename QueryTypeName>
  QueryTypeName* cast()
  {
    QueryTypeName& result = dynamic_cast<QueryTypeName&>(*this);
    return &result;
  }

  inline int get_query_id() const { return query_id; }
  void set_query_id(int id);

  /**
   *  @brief Set seconds after which a query response is stale.
   *
   *  The stale seconds can be used in the expires header of a response.
   *  @param[in] stale_seconds The stale seconds as an integer.
   */
  void set_stale_seconds(const int& stale_seconds);

  void set_cached_response(const std::string& response);

  inline boost::optional<std::string> get_cached_response() const { return cached_response; }
  /**
   *  @brief Get stale seconds after which query is stale.
   *
   *  The value can be used e.g. in an expires entity-header field.
   *  Default value is 0.
   *  @return The stale seconds as an integer.
   */
  inline int get_stale_seconds() const { return stale_seconds; }

 private:
  int query_id;
  int stale_seconds;
  boost::optional<std::string> cached_response;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
