#pragma once

#include "PluginImpl.h"
#include "StoredQuery.h"
#include "StoredQueryHandlerBase.h"
#include <condition_variable>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <boost/thread/shared_mutex.hpp>
#include <boost/filesystem.hpp>
#include <json/json.h>
#include <spine/Reactor.h>
#include <macgyver/DirectoryMonitor.h>
#include <macgyver/TaskGroup.h>

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
class StoredQueryMap final
{
 public:
  StoredQueryMap(SmartMet::Spine::Reactor* theReactor, PluginImpl& plugin_impl);

  virtual ~StoredQueryMap();

  void set_background_init(bool value);

  void add_config_dir(const boost::filesystem::path& config_dir,
		      const boost::filesystem::path& template_dir);

  void wait_for_init();

  std::vector<std::string> get_handler_names() const;

  boost::shared_ptr<StoredQueryHandlerBase> get_handler_by_name(const std::string name) const;

  virtual std::vector<std::string> get_return_type_names() const;

  bool is_reload_required(bool reset = false);

  Json::Value get_constructor_map() const;

 private:
  void add_handler(boost::shared_ptr<StoredQueryHandlerBase> handler);

  void add_handler(boost::shared_ptr<StoredQueryConfig> sqh_config,
                   const boost::filesystem::path& template_dir);

  void add_handler_thread_proc(boost::shared_ptr<StoredQueryConfig> config,
                               const boost::filesystem::path& template_dir);

  void on_config_change(Fmi::DirectoryMonitor::Watcher watcher,
			const boost::filesystem::path& path,
			const boost::regex& pattern,
			const Fmi::DirectoryMonitor::Status& status);

  void on_config_error(Fmi::DirectoryMonitor::Watcher watcher,
		       const boost::filesystem::path& path,
		       const boost::regex& pattern,
		       const std::string& message);

  boost::shared_ptr<const StoredQueryHandlerBase>
    get_handler_by_file_name(const std::string& config_file_name) const;

  boost::shared_ptr<const StoredQueryConfig>
    get_query_config_by_file_name(const std::string& name) const;

  bool should_be_ignored(const StoredQueryConfig& config) const;

  boost::optional<std::string> get_ignore_reason(const StoredQueryConfig& config) const;

  void handle_query_remove(const std::string& config_file_name);

  void handle_query_add(const std::string& config_file_name,
			const boost::filesystem::path& template_dir,
			bool initial_update,
			bool silent_duplicate);

  void handle_query_modify(const std::string& config_file_name,
			   const boost::filesystem::path& template_dir);

  void handle_query_ignore(const StoredQueryConfig& config, bool initial_update);

  void request_reload(const std::string& reason);

  void enqueue_query_add(boost::shared_ptr<StoredQueryConfig> sqh_config,
			 const boost::filesystem::path& template_dir,
			 bool initial_update);

  boost::shared_ptr<StoredQueryHandlerBase> get_handler_by_name_nothrow(const std::string name) const;

  void directory_monitor_thread_proc();

 private:
  bool background_init;
  std::atomic<bool> reload_required;
  std::atomic<bool> loading_started;
  mutable boost::shared_mutex mutex;
  mutable std::mutex mutex2;
  std::condition_variable cond;
  SmartMet::Spine::Reactor* theReactor;
  PluginImpl& plugin_impl;
  std::unique_ptr<Fmi::TaskGroup> init_tasks;
  std::map<std::string, boost::shared_ptr<StoredQueryHandlerBase> > handler_map;
  std::set<std::string> duplicate;

  struct ConfigDirInfo {
    boost::filesystem::path config_dir;
    boost::filesystem::path template_dir;
    Fmi::DirectoryMonitor::Watcher watcher;
    int num_updates;
  };

  std::map<Fmi::DirectoryMonitor::Watcher, ConfigDirInfo> config_dirs;
  Fmi::DirectoryMonitor directory_monitor;
  std::thread directory_monitor_thread;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
