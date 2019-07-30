#pragma once

// ======================================================================
/*!
 * \brief SmartMet WFS plugin interface
 */
// ======================================================================

#include "Config.h"
//#include "GeoServerDB.h"
#include "PluginImpl.h"
//#include "RequestFactory.h"
//#include "StoredQueryMap.h"
//#include "XmlEnvInit.h"
//#include "XmlParser.h"

#include <spine/HTTP.h>
#include <spine/Reactor.h>
#include <spine/SmartMetPlugin.h>
#include <spine/HTTPAuthentication.h>

#include <ctpp2/CDT.hpp>

#include <macgyver/ObjectPool.h>
#include <macgyver/TimedCache.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>

#include <condition_variable>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class Plugin
  : public SmartMetPlugin
  , virtual private boost::noncopyable
  , private Xml::EnvInit
  , private SmartMet::Spine::HTTP::Authentication
{
 public:
  Plugin(SmartMet::Spine::Reactor* theReactor, const char* theConfig);
  virtual ~Plugin();

  const std::string& getPluginName() const;
  int getRequiredAPIVersion() const;

  bool reload(const char* theConfig);

 protected:
  void init();
  void shutdown();
  void requestHandler(SmartMet::Spine::Reactor& theReactor,
                      const SmartMet::Spine::HTTP::Request& theRequest,
                      SmartMet::Spine::HTTP::Response& theResponse);
  std::string getRealm() const override;

 private:
  Plugin();

  virtual bool queryIsFast(const SmartMet::Spine::HTTP::Request& theRequest) const;

 private:
  void realRequestHandler(SmartMet::Spine::Reactor& theReactor,
			  const std::string& language,
			  const SmartMet::Spine::HTTP::Request& theRequest,
			  SmartMet::Spine::HTTP::Response& theResponse);

  void adminHandler(SmartMet::Spine::Reactor& theReactor,
		    const SmartMet::Spine::HTTP::Request& theRequest,
		    SmartMet::Spine::HTTP::Response& theResponse);

  void updateLoop();

  void startUpdateLoop();
  void stopUpdateLoop();

 private:
  const std::string itsModuleName;

  boost::shared_ptr<PluginImpl> plugin_impl;

  SmartMet::Spine::Reactor* itsReactor;

  const char* itsConfig;

  std::atomic<bool> itsShuttingDown;
  std::atomic<bool> itsReloading;
  std::unique_ptr<std::thread> itsUpdateLoopThread;
  std::condition_variable itsUpdateNotifyCond;
  std::mutex itsUpdateNotifyMutex;
};  // class Plugin

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
