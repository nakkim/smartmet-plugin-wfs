// ======================================================================
/*!
 * \brief SmartMet Wfs plugin implementation
 */
// ======================================================================

#include "ErrorResponseGenerator.h"
#include "StoredQueryConfig.h"
#include "StoredQueryHandlerFactoryDef.h"
#include "WfsConst.h"
#include "WfsConvenience.h"
#include "WfsException.h"
#include "XmlParser.h"
#include "XmlUtils.h"
#include "request/DescribeFeatureType.h"
#include "request/DescribeStoredQueries.h"
#include "request/GetCapabilities.h"
#include "request/GetFeature.h"
#include "request/GetPropertyValue.h"
#include "request/ListStoredQueries.h"
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/ref.hpp>
#include <ctpp2/CDT.hpp>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TypeName.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/FmiApiKey.h>
#include <spine/Location.h>
#include <spine/Reactor.h>
#include <spine/SmartMet.h>
#include <spine/TableFormatterFactory.h>
#include <spine/TableFormatterOptions.h>
#include <xercesc/util/PlatformUtils.hpp>
#include <Plugin.h>
#include <chrono>
#include <cxxabi.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <typeinfo>

using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;
namespace ba = boost::algorithm;
namespace bl = boost::lambda;
namespace fs = boost::filesystem;

using boost::format;
using boost::str;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
/*
 *  @brief Plugin constructor
 */
Plugin::Plugin(SmartMet::Spine::Reactor* theReactor, const char* theConfig)
    : SmartMetPlugin(),
      itsModuleName("WFS"),
      itsReactor(theReactor),
      itsConfig(theConfig),
      itsShutdownRequested(false),
      itsUpdateLoopThreadCount(0)
{
}

void Plugin::init()
{
  try
  {
    try
    {
      plugin_data.reset(new PluginData(itsReactor, itsConfig));
      query_cache.reset(new QueryResponseCache(
          plugin_data->get_config().getCacheSize(),
          std::chrono::seconds(plugin_data->get_config().getCacheTimeConstant())));
      request_factory.reset(new RequestFactory(*plugin_data));

      if (itsReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
      {
        std::ostringstream msg;
        msg << "WFS Plugin and Server SmartMet API version mismatch"
            << " (plugin API version is " << SMARTMET_API_VERSION << ", server requires "
            << itsReactor->getRequiredAPIVersion() << ")";
        throw SmartMet::Spine::Exception(BCP, msg.str());
      }

      request_factory
          ->register_request_type(
              "GetCapabilities",
              "",
              boost::bind(&Plugin::parse_kvp_get_capabilities_request, this, _1, _2),
              boost::bind(&Plugin::parse_xml_get_capabilities_request, this, _1, _2, _3))
          .register_request_type(
              "DescribeFeatureType",
              "supportsDescribeFeatureType",
              boost::bind(&Plugin::parse_kvp_describe_feature_type_request, this, _1, _2),
              boost::bind(&Plugin::parse_xml_describe_feature_type_request, this, _1, _2, _3))
          .register_request_type(
              "GetFeature",
              "supportsGetFeature",
              boost::bind(&Plugin::parse_kvp_get_feature_request, this, _1, _2),
              boost::bind(&Plugin::parse_xml_get_feature_request, this, _1, _2, _3))
          .register_request_type(
              "GetPropertyValue",
              "supportsGetPropertyValue",
              boost::bind(&Plugin::parse_kvp_get_property_value_request, this, _1, _2),
              boost::bind(&Plugin::parse_xml_get_property_value_request, this, _1, _2, _3))
          .register_request_type(
              "ListStoredQueries",
              "supportsListStoredQueries",
              boost::bind(&Plugin::parse_kvp_list_stored_queries_request, this, _1, _2),
              boost::bind(&Plugin::parse_xml_list_stored_queries_request, this, _1, _2, _3))
          .register_request_type(
              "DescribeStoredQueries",
              "supportsDescribeStoredQueries",
              boost::bind(&Plugin::parse_kvp_describe_stored_queries_request, this, _1, _2),
              boost::bind(&Plugin::parse_xml_describe_stored_queries_request, this, _1, _2, _3))
          .register_unimplemented_request_type("LockFeature")
          .register_unimplemented_request_type("GetFeatureWithLock")
          .register_unimplemented_request_type("CreateStoredQuery")
          .register_unimplemented_request_type("DropStoredQuery")
          .register_unimplemented_request_type("Transaction");

      const std::vector<std::string>& languages = plugin_data->get_languages();
      BOOST_FOREACH (const auto& language, languages)
      {
        const std::string url = plugin_data->get_config().defaultUrl() + "/" + language;
        if (!itsReactor->addContentHandler(
                this, url, boost::bind(&Plugin::realRequestHandler, this, _1, language, _2, _3)))
        {
          std::ostringstream msg;
          msg << "Failed to register WFS content handler for language '" << language << "'";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }
      }

      // Begin the update loop if enabled
      if (plugin_data->get_config().getEnableConfigurationPolling())
        itsUpdateLoopThread.reset(new std::thread(std::bind(&Plugin::updateLoop, this)));
    }
    catch (...)
    {
      throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
    }

    if (!itsReactor->addContentHandler(
            this,
            plugin_data->get_config().defaultUrl(),
            boost::bind(&Plugin::realRequestHandler, this, _1, "", _2, _3)))
    {
      throw SmartMet::Spine::Exception(
          BCP, "Failed to register WFS content handler for default language");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Init failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the plugin
 */
// ----------------------------------------------------------------------

void Plugin::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (wfs)\n";
    itsShutdownRequested = true;

    while (itsUpdateLoopThreadCount > 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (itsUpdateLoopThread and itsUpdateLoopThread->joinable())
      itsUpdateLoopThread->join();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Starting shutdown failed!");
  }
}
// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin() {}
// ----------------------------------------------------------------------
/*!
 * \brief Return the plugin name
 */
// ----------------------------------------------------------------------

const std::string& Plugin::getPluginName() const
{
  return itsModuleName;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the required version
 */
// ----------------------------------------------------------------------

int Plugin::getRequiredAPIVersion() const
{
  return SMARTMET_API_VERSION;
}
struct Plugin::RequestResult
{
  SmartMet::Spine::HTTP::Status status;
  bool may_validate_xml;
  std::ostringstream output;
  boost::optional<int> expires_seconds;

 public:
  RequestResult() : status(SmartMet::Spine::HTTP::not_a_status), may_validate_xml(true), output() {}
};

/**
 *  @brief Perform actual WFS request and generate the response
 */
void Plugin::query(const std::string& req_language,
                   const SmartMet::Spine::HTTP::Request& req,
                   Plugin::RequestResult& result)
{
  try
  {
    const auto method = req.getMethod();

    std::string hostname;
    if (const auto header_x_forwarded_host = req.getHeader("X-Forwarded-Host"))
      hostname = *header_x_forwarded_host;
    else if (const auto header_host = req.getHeader("Host"))
      hostname = *header_host;
    else
      hostname = plugin_data->get_fallback_hostname();

    std::string protocol;
    if (const auto header_x_forwarded_protocol = req.getProtocol())
      protocol = *header_x_forwarded_protocol;
    else
      protocol = plugin_data->get_fallback_protocol();

    const std::string fmi_apikey_prefix = "/fmi-apikey/";

    const std::string language =
        req_language == "" ? *plugin_data->get_config().get_languages().begin() : req_language;

    if (method == SmartMet::Spine::HTTP::RequestMethod::GET)
    {
      boost::shared_ptr<RequestBase> request = request_factory->parse_kvp(language, req);
      request->set_hostname(hostname);
      request->set_protocol(protocol);
      auto fmi_apikey = get_fmi_apikey(req);
      if (fmi_apikey)
      {
        request->set_fmi_apikey_prefix(fmi_apikey_prefix);
        request->set_fmi_apikey(*fmi_apikey);
      }
      result.may_validate_xml = request->may_validate_xml();
      request->execute(result.output);
      result.expires_seconds = request->get_response_expires_seconds();
      if (request->get_http_status())
        result.status = request->get_http_status();
    }

    else if (method == SmartMet::Spine::HTTP::RequestMethod::POST)
    {
      const std::string content_type = get_mandatory_header(req, "Content-Type");

      auto fmi_apikey = get_fmi_apikey(req);

      if (content_type == "application/x-www-form-urlencoded")
      {
        boost::shared_ptr<RequestBase> request = request_factory->parse_kvp(language, req);
        request->set_hostname(hostname);
        request->set_protocol(protocol);
        if (fmi_apikey)
        {
          request->set_fmi_apikey_prefix(fmi_apikey_prefix);
          request->set_fmi_apikey(*fmi_apikey);
        }
        result.may_validate_xml = request->may_validate_xml();
        request->execute(result.output);
        result.expires_seconds = request->get_response_expires_seconds();
        if (request->get_http_status())
          result.status = request->get_http_status();
      }
      else if (content_type == "text/xml")
      {
        const std::string& content = req.getContent();
        if (content == "")
        {
          SmartMet::Spine::Exception exception(BCP, "No request content available!");
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
          throw exception;
        }

        boost::shared_ptr<xercesc::DOMDocument> xml_doc;
        Xml::Parser::RootElemInfo root_info;
        try
        {
          Xml::Parser::root_element_cb_t root_element_cb = (bl::var(root_info) = bl::_1);
          xml_doc = get_xml_parser()->parse_string(content, "WFS", root_element_cb);
        }
        catch (const Xml::XmlError& orig_err)
        {
          const std::list<std::string>& messages = orig_err.get_messages();

          if ((orig_err.get_error_level() != Xml::XmlError::FATAL_ERROR) and
              (root_info.nqname != "") and (root_info.ns_uri == WFS_NAMESPACE_URI) and
              request_factory->check_request_name(root_info.nqname))
          {
            if (root_info.attr_map.count("service") == 0)
            {
              SmartMet::Spine::Exception exception(
                  BCP, "Missing the 'service' attribute!", nullptr);
              exception.addDetails(messages);
              if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
                exception.addParameter(WFS_EXCEPTION_CODE, WFS_MISSING_PARAMETER_VALUE);
              exception.addParameter(WFS_LOCATION, "service");
              exception.addParameter(WFS_LANGUAGE, req_language);
              throw exception;
            }
            else if (root_info.attr_map.at("service") != "WFS")
            {
              SmartMet::Spine::Exception exception(
                  BCP, "Incorrect value for the 'service' attribute!", nullptr);
              exception.addDetails(messages);
              if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
                exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
              exception.addParameter(WFS_LOCATION, "service");
              exception.addParameter(WFS_LANGUAGE, req_language);
              exception.addParameter("Attribute value", root_info.attr_map.at("service"));
              exception.addParameter("Expected value", "WFS");
              throw exception;
            }

            if (root_info.attr_map.count("version") == 0)
            {
              SmartMet::Spine::Exception exception(
                  BCP, "Missing the 'version' attribute!", nullptr);
              exception.addDetails(messages);
              if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
                exception.addParameter(WFS_EXCEPTION_CODE, WFS_MISSING_PARAMETER_VALUE);
              exception.addParameter(WFS_LOCATION, "version");
              exception.addParameter(WFS_LANGUAGE, req_language);
              throw exception;
            }
            else if (root_info.attr_map.at("version") != "2.0.0")
            {
              SmartMet::Spine::Exception exception(
                  BCP, "Incorrect value for the 'version' attribute!", nullptr);
              exception.addDetails(messages);
              if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
                exception.addParameter(WFS_EXCEPTION_CODE, WFS_INVALID_PARAMETER_VALUE);
              exception.addParameter(WFS_LOCATION, "version");
              exception.addParameter(WFS_LANGUAGE, req_language);
              exception.addParameter("Attribute value", root_info.attr_map.at("version"));
              exception.addParameter("Expected value", "2.0.0");
              throw exception;
            }
          }

          SmartMet::Spine::Exception exception(
              BCP, "Parsing of the incoming XML request failed", nullptr);
          exception.addDetails(messages);
          if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
          exception.addParameter(WFS_LANGUAGE, req_language);
          throw exception;
        }

        boost::shared_ptr<RequestBase> request = request_factory->parse_xml(language, *xml_doc);
        request->set_hostname(hostname);
        request->set_protocol(protocol);
        if (fmi_apikey)
        {
          request->set_fmi_apikey_prefix(fmi_apikey_prefix);
          request->set_fmi_apikey(*fmi_apikey);
        }
        result.may_validate_xml = request->may_validate_xml();
        request->execute(result.output);
        result.expires_seconds = request->get_response_expires_seconds();
        if (request->get_http_status())
          result.status = request->get_http_status();
      }
      else
      {
        SmartMet::Spine::Exception exception(BCP, "Unsupported content type!");
        if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
          exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
        exception.addParameter(WFS_LANGUAGE, req_language);
        exception.addParameter("Requested content type", content_type);
        throw exception;
      }
    }
    else
    {
      SmartMet::Spine::Exception exception(
          BCP, "HTTP method '" + req.getMethodString() + "' is not supported!");
      if (exception.getExceptionByParameterName(WFS_EXCEPTION_CODE) == nullptr)
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      exception.addParameter(WFS_LANGUAGE, req_language);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Query failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main content handler
 */
// ----------------------------------------------------------------------

void Plugin::requestHandler(SmartMet::Spine::Reactor& theReactor,
                            const SmartMet::Spine::HTTP::Request& theRequest,
                            SmartMet::Spine::HTTP::Response& theResponse)
{
  try
  {
    std::string language = "eng";
    std::string defaultPath = plugin_data->get_config().defaultUrl();
    std::string requestPath = theRequest.getResource();

    if ((defaultPath.length() + 2) < requestPath.length())
      language = requestPath.substr(defaultPath.length() + 1, 3);

    this->realRequestHandler(theReactor, language, theRequest, theResponse);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Plugin::realRequestHandler(SmartMet::Spine::Reactor& /* theReactor */,
                                const std::string& language,
                                const SmartMet::Spine::HTTP::Request& theRequest,
                                SmartMet::Spine::HTTP::Response& theResponse)
{
  try
  {
    // Now

    ptime t_now = second_clock::universal_time();

    try
    {
      std::ostringstream output;
      RequestResult result;
      query(language, theRequest, result);
      const std::string& content = result.output.str();
      theResponse.setContent(content);
      auto status = result.status ? result.status : SmartMet::Spine::HTTP::ok;
      theResponse.setStatus(status);

      // Latter (false) should newer happen.
      const int expires_seconds = (result.expires_seconds)
                                      ? result.expires_seconds.get()
                                      : plugin_data->get_config().getDefaultExpiresSeconds();

      // Build cache expiration time info

      ptime t_expires = t_now + seconds(expires_seconds);

      // The headers themselves

      boost::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));

      // std::string mime = "text/xml; charset=UTF-8";
      const std::string mime =
          content.substr(0, 6) == "<html>" ? "text/html; charset=UTF-8" : "text/xml; charset=UTF-8";
      theResponse.setHeader("Content-Type", mime.c_str());

      if (theResponse.getContentLength() == 0)
      {
        std::ostringstream msg;
        msg << "Warning: Empty input for request " << theRequest.getQueryString() << " from "
            << theRequest.getClientIP() << std::endl;
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
        throw exception;
      }

      if (status == SmartMet::Spine::HTTP::ok)
      {
        std::string cachecontrol = "public, max-age=" + Fmi::to_string(expires_seconds);
        std::string expiration = tformat->format(t_expires);
        std::string modification = tformat->format(t_now);

        theResponse.setHeader("Cache-Control", cachecontrol.c_str());
        theResponse.setHeader("Expires", expiration.c_str());
        theResponse.setHeader("Last-Modified", modification.c_str());
        theResponse.setHeader("Access-Control-Allow-Origin", "*");
      }

      if (result.may_validate_xml)
        maybe_validate_output(theRequest, theResponse);
    }
    catch (...)
    {
      // Catching all exceptions

      SmartMet::Spine::Exception exception(BCP, "Request processing exception!", nullptr);
      exception.addParameter("URI", theRequest.getURI());
      exception.printError();

      // FIXME: implement correct processing phase support (parsing, processing)
      ErrorResponseGenerator error_response_generator(*plugin_data);
      const auto error_response = error_response_generator.create_error_response(
          ErrorResponseGenerator::REQ_PROCESSING, theRequest);
      theResponse.setContent(error_response.response);
      theResponse.setStatus(error_response.status);
      theResponse.setHeader("Content-Type", "text/xml; charset=UTF8");
      theResponse.setHeader("Access-Control-Allow-Origin", "*");
      theResponse.setHeader("X-WFS-Error", error_response.wfs_err_code);
      maybe_validate_output(theRequest, theResponse);
      std::cerr << error_response.log_message;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Plugin::queryIsFast(const SmartMet::Spine::HTTP::Request& /* theRequest */) const
{
  // FIXME: implement test
  return false;
}

void Plugin::maybe_validate_output(const SmartMet::Spine::HTTP::Request& req,
                                   SmartMet::Spine::HTTP::Response& response) const
{
  try
  {
    if (plugin_data->get_config().getValidateXmlOutput())
    {
      try
      {
        const std::string content = response.getContent();
        std::size_t pos = content.find_first_not_of(" \t\r\n");
        if (pos != std::string::npos)
        {
          if (ba::iequals(content.substr(pos, 6), "<html>") or content.substr(pos, 1) != "<")
          {
            return;
          }
        }
        else
        {
          return;
        }

        get_xml_parser()->parse_string(content);
      }
      catch (const Xml::XmlError& err)
      {
        std::ostringstream msg;
        msg << SmartMet::Spine::log_time_str()
            << " [WFS] [ERROR] XML Response validation failed: " << err.what() << '\n';
        BOOST_FOREACH (const std::string& err_msg, err.get_messages())
        {
          msg << "       XML: " << err_msg << std::endl;
        }
        const std::string req_str = req.toString();
        std::vector<std::string> lines;
        ba::split(lines, req_str, ba::is_any_of("\n"));
        msg << "   WFS request:\n";
        BOOST_FOREACH (const auto& line, lines)
        {
          msg << "       " << ba::trim_right_copy_if(line, ba::is_any_of(" \t\r\n")) << '\n';
        }
        std::cout << msg.str() << std::flush;
      }
    }
#ifdef WFS_DEBUG
    else
    {
      try
      {
        const std::string content = response.getContent();
        if (content.substr(0, 5) == "<?xml")
        {
          Xml::str2xmldom(response.getContent(), "wfs_query_tmp");
        }
      }
      catch (const Xml::XmlError& err)
      {
        std::cout << "\nXML: non validating XML response read failed\n";
        std::cout << "XML: " << err.what() << std::endl;
        BOOST_FOREACH (const std::string& msg, err.get_messages())
        {
          std::cout << "XML: " << msg << std::endl;
        }
      }
    }
#endif
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::optional<std::string> Plugin::get_fmi_apikey(
    const SmartMet::Spine::HTTP::Request& theRequest) const
{
  try
  {
    return SmartMet::Spine::FmiApiKey::getFmiApiKey(theRequest, true);
  }
  catch (...)
  {
    throw;
  }
}

Xml::Parser* Plugin::get_xml_parser() const
{
  try
  {
    return plugin_data->get_xml_parser()->get();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_kvp_get_capabilities_request(
    const std::string& language, const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    return Request::GetCapabilities::create_from_kvp(language, request, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_xml_get_capabilities_request(const std::string& language,
                                                        const xercesc::DOMDocument& document,
                                                        const xercesc::DOMElement& root)
{
  try
  {
    (void)root;

    return Request::GetCapabilities::create_from_xml(language, document, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_kvp_describe_feature_type_request(
    const std::string& language, const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    return Request::DescribeFeatureType::create_from_kvp(language, request, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_xml_describe_feature_type_request(const std::string& language,
                                                             const xercesc::DOMDocument& document,
                                                             const xercesc::DOMElement& root)
{
  try
  {
    (void)root;
    return Request::DescribeFeatureType::create_from_xml(language, document, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_kvp_get_feature_request(const std::string& language,
                                                   const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    return Request::GetFeature::create_from_kvp(language, request, *plugin_data, *query_cache);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_xml_get_feature_request(const std::string& language,
                                                   const xercesc::DOMDocument& document,
                                                   const xercesc::DOMElement& root)
{
  try
  {
    (void)root;
    return Request::GetFeature::create_from_xml(language, document, *plugin_data, *query_cache);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_kvp_get_property_value_request(
    const std::string& language, const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    return Request::GetPropertyValue::create_from_kvp(
        language, request, *plugin_data, *query_cache);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_xml_get_property_value_request(const std::string& language,
                                                          const xercesc::DOMDocument& document,
                                                          const xercesc::DOMElement& root)
{
  try
  {
    (void)root;
    return Request::GetPropertyValue::create_from_xml(
        language, document, *plugin_data, *query_cache);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_kvp_list_stored_queries_request(
    const std::string& language, const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    return Request::ListStoredQueries::create_from_kvp(language, request, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_xml_list_stored_queries_request(const std::string& language,
                                                           const xercesc::DOMDocument& document,
                                                           const xercesc::DOMElement& root)
{
  try
  {
    (void)root;
    return Request::ListStoredQueries::create_from_xml(language, document, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_kvp_describe_stored_queries_request(
    const std::string& language, const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    return Request::DescribeStoredQueries::create_from_kvp(language, request, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

RequestBaseP Plugin::parse_xml_describe_stored_queries_request(const std::string& language,
                                                               const xercesc::DOMDocument& document,
                                                               const xercesc::DOMElement& root)
{
  try
  {
    (void)root;
    return Request::DescribeStoredQueries::create_from_xml(language, document, *plugin_data);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void Plugin::updateLoop()
{
  try
  {
    itsUpdateLoopThreadCount++;
    while (not(itsShutdownRequested or isShutdownRequested()))
    {
      try
      {
        for (int i = 0; (not itsShutdownRequested && i < 5); i++)
          boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

        if (not itsShutdownRequested)
          plugin_data->updateStoredQueryMap(itsReactor);
      }
      catch (...)
      {
        Spine::Exception exception(BCP, "Stored query map update failed!", nullptr);
        exception.printError();
      }
    }
    itsUpdateLoopThreadCount--;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Capabilities update failed!");
  }
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

/*
 * Server knows us through the 'SmartMetPlugin' virtual interface, which
 * the 'Plugin' class implements.
 */

extern "C" SmartMetPlugin* create(SmartMet::Spine::Reactor* them, const char* config)
{
  return new SmartMet::Plugin::WFS::Plugin(them, config);
}

extern "C" void destroy(SmartMetPlugin* us)
{
  // This will call 'Plugin::~Plugin()' since the destructor is virtual
  delete us;
}

// ======================================================================
