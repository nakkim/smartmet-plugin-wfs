#pragma once

#include "PluginImpl.h"
#include "StoredQuery.h"
#include "StoredQueryHandlerBase.h"
#include <boost/filesystem.hpp>
#include <spine/Reactor.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class PluginImpl;
class StoredQueryConfig;
class StoredQueryHandlerBase;

/**
 *  @brief A C++ class for holding information about available WFS stored queries
 *         and mapping query names to actual objects which handles the queries.
 */
class StoredQueryMap
{
  class Init;

 public:
  StoredQueryMap();

  virtual ~StoredQueryMap();

  void set_background_init(bool value);

  void add_handler(boost::shared_ptr<StoredQueryHandlerBase> handler);

  void add_handler(SmartMet::Spine::Reactor* theReactor,
                   const boost::filesystem::path& stored_query_config_file,
                   const boost::filesystem::path& template_dir,
                   PluginImpl& plugin_impl);

  void read_config_dir(SmartMet::Spine::Reactor* theReactor,
                       const boost::filesystem::path& config_dir,
                       const boost::filesystem::path& template_dir,
                       PluginImpl& plugin_impl);

  inline std::vector<std::string> get_handler_names() const { return handler_names; }
  boost::shared_ptr<StoredQueryHandlerBase> get_handler_by_name(const std::string name) const;

  virtual std::vector<std::string> get_return_type_names() const;

  void update_handlers(Spine::Reactor* theReactor, PluginImpl& plugin_impl);

 private:
  void add_handler(SmartMet::Spine::Reactor* theReactor,
                   boost::shared_ptr<StoredQueryConfig> sqh_config,
                   const boost::filesystem::path& template_dir,
                   PluginImpl& plugin_impl);

  void add_handler_thread_proc(SmartMet::Spine::Reactor* theReactor,
                               boost::shared_ptr<StoredQueryConfig> config,
                               const boost::filesystem::path& template_dir,
                               PluginImpl& plugin_impl);

 private:
  bool background_init;
  mutable SmartMet::Spine::MutexType mutex;
  std::vector<std::string> handler_names;
  std::map<std::string, boost::shared_ptr<StoredQueryHandlerBase> > handler_map;
  std::unique_ptr<Init> init;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
