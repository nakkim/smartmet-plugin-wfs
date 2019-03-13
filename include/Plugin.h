#pragma once

// ======================================================================
/*!
 * \brief SmartMet WFS plugin interface
 */
// ======================================================================

#include "Config.h"
#include "GeoServerDB.h"
#include "PluginData.h"
#include "RequestFactory.h"
#include "StoredQueryMap.h"
#include "XmlEnvInit.h"
#include "XmlParser.h"

#include <spine/HTTP.h>
#include <spine/Reactor.h>
#include <spine/SmartMetPlugin.h>

#include <ctpp2/CDT.hpp>

#include <macgyver/ObjectPool.h>
#include <macgyver/TimedCache.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>

#include <iostream>
#include <list>
#include <map>
#include <string>
#include <thread>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class Plugin : public SmartMetPlugin, virtual private boost::noncopyable, private Xml::EnvInit
{
  struct RequestResult;

 public:
  Plugin(SmartMet::Spine::Reactor* theReactor, const char* theConfig);
  virtual ~Plugin();

  const std::string& getPluginName() const;
  int getRequiredAPIVersion() const;

 protected:
  void init();
  void shutdown();
  void requestHandler(SmartMet::Spine::Reactor& theReactor,
                      const SmartMet::Spine::HTTP::Request& theRequest,
                      SmartMet::Spine::HTTP::Response& theResponse);

 private:
  Plugin();

  void query(const std::string& language,
             const SmartMet::Spine::HTTP::Request& req,
             RequestResult& result);

  virtual bool queryIsFast(const SmartMet::Spine::HTTP::Request& theRequest) const;

 private:
  virtual void realRequestHandler(SmartMet::Spine::Reactor& theReactor,
                                  const std::string& language,
                                  const SmartMet::Spine::HTTP::Request& theRequest,
                                  SmartMet::Spine::HTTP::Response& theResponse);

  void maybe_validate_output(const SmartMet::Spine::HTTP::Request& req,
                             SmartMet::Spine::HTTP::Response& response) const;

  boost::optional<std::string> get_fmi_apikey(
      const SmartMet::Spine::HTTP::Request& theRequest) const;

  Xml::Parser* get_xml_parser() const;

  RequestBaseP parse_kvp_get_capabilities_request(const std::string& language,
                                                  const SmartMet::Spine::HTTP::Request& request);

  RequestBaseP parse_xml_get_capabilities_request(const std::string& language,
                                                  const xercesc::DOMDocument& document,
                                                  const xercesc::DOMElement& root);

  RequestBaseP parse_kvp_describe_feature_type_request(
      const std::string& language, const SmartMet::Spine::HTTP::Request& request);

  RequestBaseP parse_xml_describe_feature_type_request(const std::string& language,
                                                       const xercesc::DOMDocument& document,
                                                       const xercesc::DOMElement& root);

  RequestBaseP parse_kvp_get_feature_request(const std::string& language,
                                             const SmartMet::Spine::HTTP::Request& request);

  RequestBaseP parse_xml_get_feature_request(const std::string& language,
                                             const xercesc::DOMDocument& document,
                                             const xercesc::DOMElement& root);

  RequestBaseP parse_kvp_get_property_value_request(const std::string& language,
                                                    const SmartMet::Spine::HTTP::Request& request);

  RequestBaseP parse_xml_get_property_value_request(const std::string& language,
                                                    const xercesc::DOMDocument& document,
                                                    const xercesc::DOMElement& root);

  RequestBaseP parse_kvp_list_stored_queries_request(const std::string& language,
                                                     const SmartMet::Spine::HTTP::Request& request);

  RequestBaseP parse_xml_list_stored_queries_request(const std::string& language,
                                                     const xercesc::DOMDocument& document,
                                                     const xercesc::DOMElement& root);

  RequestBaseP parse_kvp_describe_stored_queries_request(
      const std::string& language, const SmartMet::Spine::HTTP::Request& request);

  RequestBaseP parse_xml_describe_stored_queries_request(const std::string& language,
                                                         const xercesc::DOMDocument& document,
                                                         const xercesc::DOMElement& root);

 private:
  boost::shared_ptr<pqxx::connection> create_geoserver_db_conn() const;

  void updateLoop();

 private:
  const std::string itsModuleName;

  std::unique_ptr<PluginData> plugin_data;

  /**
   *   @brief An object that reads actual requests and creates request objects
   */
  std::unique_ptr<RequestFactory> request_factory;

  SmartMet::Spine::Reactor* itsReactor;

  const char* itsConfig;

  boost::atomic<bool> itsShutdownRequested;
  boost::atomic<int> itsUpdateLoopThreadCount;

  std::unique_ptr<std::thread> itsUpdateLoopThread;

};  // class Plugin

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
