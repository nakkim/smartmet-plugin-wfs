#pragma once

#include <spine/HTTP.h>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

class PluginImpl;

/**
 *   @brief Abstract base class for WFS requests
 *
 *   Some examples are
 *   - GetFeature
 *   - ListStoredQueries
 *   - DescribeStoredQueries
 */
class RequestBase
{
 public:
  enum RequestType
  {
    DESCRIBE_FEATURE_TYPE,
    DESCRIBE_STORED_QUERIES,
    GET_CAPABILITIES,
    GET_FEATURE,
    GET_PROPERTY_VALUE,
    LIST_STORED_QUERIES
  };

 public:
  RequestBase(const std::string& language, const PluginImpl& plugin_impl);

  virtual ~RequestBase();

  void set_is_soap_request(bool value);

  inline bool is_soap_request() const { return soap_request; }
  /**
   *   @brief Get seconds after which response is considered expire.
   *
   *   The expire value is meant to use in the Expires header
   *   of a response (e.g. the max-age cache-control directive).
   *   For new reques types defaultExpiresSeconds can be used from
   *   the main configuration.
   *   @return Seconds as an integer.
   */
  virtual int get_response_expires_seconds() const = 0;

  /**
   *   @brief Get type of the request
   */
  virtual RequestType get_type() const = 0;

  /**
   *   @brief Query language string provided in contructor
   */
  const std::string& get_language() const { return language; }
  /**
   *   @brief Execute the request and return the result as the
   *          string in the case of a success.
   */
  virtual void execute(std::ostream& ost) const = 0;

  /**
   *   @brief Perform dynamic cast to corresponding request type
   *
   *   Throws std::bad_cast if dynamic_cast fails
   */
  template <typename RequestTypeName>
  RequestTypeName* cast()
  {
    RequestTypeName& result = dynamic_cast<RequestTypeName&>(*this);
    return &result;
  }

  /**
   *   @brief Extract the request name and namespace URI from DOM document
   *
   *   The first element of returned pair contains the name and the second one -
   *   namespace uri.
   */
  static std::string extract_request_name(const xercesc::DOMDocument& document);

  /**
   *   @brief Extract the request name and namespace URI from KVP request
   */
  static std::string extract_request_name(const SmartMet::Spine::HTTP::Request& http_request);

  /**
   *   @brief Specifies whether response XML may be validated
   *
   *   Note that response XML validation may be disabled in configuration.
   *   The result of this virtual method only allows or disallow
   *   validation if it is already enabled in configuration
   */
  virtual bool may_validate_xml() const;

  virtual void set_fmi_apikey(const std::string& fmi_apikey);

  virtual void set_fmi_apikey_prefix(const std::string& fmi_apikey_prefix);

  virtual void set_hostname(const std::string& hostname);

  virtual void set_protocol(const std::string& protocol);

  boost::optional<std::string> get_fmi_apikey() const { return fmi_apikey; }
  boost::optional<std::string> get_hostname() const { return hostname; }
  boost::optional<std::string> get_protocol() const { return protocol; }
  void substitute_all(const std::string& src, std::ostream& output) const;

  void set_http_status(SmartMet::Spine::HTTP::Status status) const;

  inline SmartMet::Spine::HTTP::Status get_http_status() const { return status; }

 protected:
  /**
   *   @brief Verifies that the request name from HTTP request is correct
   */
  static void check_request_name(const SmartMet::Spine::HTTP::Request& request,
                                 const std::string& name);

  /**
   *   @brief Verifies that the request name from XML request is correct
   */
  static void check_request_name(const xercesc::DOMDocument& document, const std::string& name);

  /**
   *   @brief Verifies presence and values of mandatory attributes of root element
   */
  virtual void check_mandatory_attributes(const xercesc::DOMDocument& document);

  /**
   *   @brief Get DOM document root (here only to have error checking in one place)
   */
  static const xercesc::DOMElement* get_xml_root(const xercesc::DOMDocument& document);

  static void check_output_format_attribute(const std::string& value);

  /**
   *   @brief Check WFS version (KVP only)
   *
   *   Require it to be 2.0.0 if provided.
   */
  static void check_wfs_version(const SmartMet::Spine::HTTP::Request& request);

 protected:
  const PluginImpl& plugin_impl;

 private:
  const std::string language;

  /**
   *   Specifies whether the request is generated from SOAP request
   */
  bool soap_request;

  boost::optional<std::string> fmi_apikey;
  boost::optional<std::string> fmi_apikey_prefix;
  boost::optional<std::string> hostname;
  boost::optional<std::string> protocol;

  mutable SmartMet::Spine::HTTP::Status status;
};

typedef boost::shared_ptr<RequestBase> RequestBaseP;

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
