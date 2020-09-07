#include "StoredQueryMap.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/chrono.hpp>
#include <boost/filesystem.hpp>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;
namespace fs = boost::filesystem;

namespace
{
  bool is_equal(const std::string& first, const std::string& second)
  {
    return first == second;
  }
}  // namespace

bw::StoredQueryMap::StoredQueryMap(SmartMet::Spine::Reactor* theReactor, PluginImpl& plugin_impl)
  : background_init(false)
  , shutdown_requested(false)
  , reload_required(false)
  , loading_started(false)
  , theReactor(theReactor)
  , plugin_impl(plugin_impl)
{
}

bw::StoredQueryMap::~StoredQueryMap()
{
  if (directory_monitor_thread.joinable()) {
    directory_monitor.stop();
    directory_monitor_thread.join();
  }
}

void bw::StoredQueryMap::shutdown()
{
    shutdown_requested = true;
  if (init_tasks) {
    init_tasks->stop();
  }
}

void bw::StoredQueryMap::set_background_init(bool value)
{
  background_init = value;
  if (background_init) {
    init_tasks.reset(new Fmi::AsyncTaskGroup(10));
  } else {
    init_tasks.reset();
  }
}

void bw::StoredQueryMap::add_config_dir(const boost::filesystem::path& config_dir,
					const boost::filesystem::path& template_dir)
{
  ConfigDirInfo ci;
  ci.num_updates = 0;
  ci.config_dir = config_dir;
  ci.template_dir = template_dir;
  ci.watcher = directory_monitor
    .watch(config_dir,
	   boost::bind(&bw::StoredQueryMap::on_config_change, this, ::_1, ::_2, ::_3, ::_4),
	   boost::bind(&bw::StoredQueryMap::on_config_error, this, ::_1, ::_2, ::_3, ::_4),
	   5, Fmi::DirectoryMonitor::CREATE | Fmi::DirectoryMonitor::DELETE | Fmi::DirectoryMonitor::MODIFY);
  if (config_dirs.empty()) {
    std::thread tmp(std::bind(&bw::StoredQueryMap::directory_monitor_thread_proc, this));
    directory_monitor_thread.swap(tmp);
  }
  config_dirs[ci.watcher] = ci;
}

void bw::StoredQueryMap::wait_for_init()
{
  do {
    std::time_t start = std::time(nullptr);
    std::unique_lock<std::mutex> lock(mutex2);
    while (not shutdown_requested and not directory_monitor.ready()) {
      cond.wait_for(lock, std::chrono::milliseconds(100));
      if (not loading_started and (std::time(nullptr) - start > 180)) {
	throw SmartMet::Spine::Exception::Trace(BCP, "Timed out while waiting for stored query configuration loading to start");
      }
    }
  } while (false);

  if (init_tasks) {
    init_tasks->wait();
  }

  std::cout << SmartMet::Spine::log_time_str() << ": [WFS] Initial loading of stored query configuration files finished"
	    << std::endl;
}

bool bw::StoredQueryMap::is_reload_required(bool reset)
{
  if (reset) {
    bool response = reload_required.exchange(false);
    if (response) {
      std::cout << SmartMet::Spine::log_time_str() << ": [WFS] Cleared reload required flag" << std::endl;
    }
    return response;
  } else {
    return reload_required;
  }
}

void bw::StoredQueryMap::add_handler(boost::shared_ptr<StoredQueryHandlerBase> handler)
{
  try
  {
    boost::unique_lock<boost::shared_mutex> lock(mutex);
    const std::string name = handler->get_query_name();
    std::string lname = Fmi::ascii_tolower_copy(handler->get_query_name());

    auto find_iter = handler_map.find(lname);
    if (find_iter != handler_map.end())
    {
      // Override already defined stored query with the current one
      handler_map.erase(find_iter);
    }

    handler_map.insert(std::make_pair(lname, handler));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<bw::StoredQueryHandlerBase> bw::StoredQueryMap::get_handler_by_name(
    const std::string name) const
{
  try
  {
    const std::string& lname = Fmi::ascii_tolower_copy(name);
    boost::shared_lock<boost::shared_mutex> lock(mutex);
    auto loc = handler_map.find(lname);
    if (loc == handler_map.end())
    {
      SmartMet::Spine::Exception exception(BCP, "No handler for '" + name + "' found!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception.disableStackTrace();
    }
    else
    {
      return loc->second;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string> bw::StoredQueryMap::get_return_type_names() const
{
  try
  {
    std::set<std::string> return_type_set;
    boost::shared_lock<boost::shared_mutex> lock(mutex);
    BOOST_FOREACH (const auto& handler_map_item, handler_map)
    {
      // NOTE: Cannot call StoredQueryHandlerBase::get_return_type_names() here to
      //       avoid recursion as hander itself may call StoredQueryMap::get_return_type_names().
      //       One must take types from stored queries configuration instead.
      const std::vector<std::string> return_types =
          handler_map_item.second->get_config()->get_return_type_names();

      std::copy(return_types.begin(),
                return_types.end(),
                std::inserter(return_type_set, return_type_set.begin()));
    }
    return std::vector<std::string>(return_type_set.begin(), return_type_set.end());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::add_handler(StoredQueryConfig::Ptr sqh_config,
                                     const boost::filesystem::path& template_dir)
{
  try
  {
    try
    {
      boost::optional<std::string> sqh_template_fn;
      if (sqh_config->have_template_fn())
      {
        sqh_template_fn = (template_dir / sqh_config->get_template_fn()).string();
      }

      boost::shared_ptr<StoredQueryHandlerBase> p_handler = StoredQueryHandlerFactoryDef::construct(
          sqh_config->get_constructor_name(), theReactor, sqh_config, plugin_impl, sqh_template_fn);

      add_handler(p_handler);

      if (plugin_impl.get_debug_level() < 1)
        return;

      std::ostringstream msg;
      std::string prefix = "";
      if (sqh_config->is_demo())
        prefix = "DEMO ";
      if (sqh_config->is_test())
        prefix = "TEST ";
      msg << SmartMet::Spine::log_time_str() << ": [WFS] [" << prefix << "stored query ready] id='"
          << p_handler->get_query_name() << "' config='" << sqh_config->get_file_name() << "'\n";
      std::cout << msg.str() << std::flush;
    }
    catch (const std::exception&)
    {
      std::ostringstream msg;
      msg << SmartMet::Spine::log_time_str()
          << ": [WFS] [ERROR] Failed to add stored query handler. Configuration '"
          << sqh_config->get_file_name() << "\n";
      std::cout << msg.str() << std::flush;

      throw SmartMet::Spine::Exception::Trace(BCP, "Failed to add stored query handler!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::add_handler_thread_proc(StoredQueryConfig::Ptr config,
                                                 const boost::filesystem::path& template_dir)
{
  try
  {
    try
    {
      add_handler(config, template_dir);
    }
    catch (const std::exception& err)
    {
      std::ostringstream msg;
      msg << SmartMet::Spine::log_time_str() << ": [WFS] [ERROR] [C++ exception of type '"
          << Fmi::current_exception_type() << "']: " << err.what() << "\n";
      msg << "### Terminated ###\n";
      std::cerr << msg.str() << std::flush;
      abort();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::on_config_change(Fmi::DirectoryMonitor::Watcher watcher,
					  const boost::filesystem::path& path,
					  const boost::regex& pattern,
					  const Fmi::DirectoryMonitor::Status& status)
{
  try {
    assert(config_dirs.count(watcher) > 0);

    loading_started = true;

    int have_errors = 0;
    const bool initial_update = config_dirs.at(watcher).num_updates == 0;
    const auto template_dir = config_dirs.at(watcher).template_dir;

    (void)path;
    (void)pattern;

    if (directory_monitor.ready() and not plugin_impl.get_config().getEnableConfigurationPolling()) {
      // Ignore changes when initial load is done and configuration polling is disabled
      return;
    }

    Fmi::DirectoryMonitor::Status s = status;

    for (auto it = s->begin(); not reload_required and it != s->end(); ++it) {
      try {
	const fs::path& fn = it->first;
	const std::string name = fn.string();
	Fmi::DirectoryMonitor::Change change = it->second;

	if ( ((change == Fmi::DirectoryMonitor::DELETE) or fs::is_regular_file(fn))
	     and not ba::starts_with(name, ".")
	     and not ba::starts_with(name, "#")
	     and ba::ends_with(name, ".conf"))
	  {
	    switch (change) {
	    case Fmi::DirectoryMonitor::DELETE:
	      handle_query_remove(name);
	      break;

	    case Fmi::DirectoryMonitor::CREATE:
	      handle_query_add(name, template_dir, initial_update, false);
	      break;

	    case Fmi::DirectoryMonitor::MODIFY:
	      handle_query_modify(name, template_dir);
	      break;

	    default:
	      if (change & (change - 1)) {
		throw SmartMet::Spine::Exception::Trace(BCP, "INTERNAL ERROR: Unsupported change type "
							+ std::to_string(int(change)));
	      }
	      break;
	    }
	  }

	config_dirs.at(watcher).num_updates++;
      } catch (...) {
        if (true || !shutdown_requested) {
	  have_errors++;
	  auto err = SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
	  std::cout << err.getStackTrace() << std::endl;
        }
      }
    }

    if (not initial_update) {
      auto tmp = duplicate;
      for (const auto& fn : tmp) {
	try {
	  handle_query_add(fn, template_dir, initial_update, true);
	} catch (...) {
	  auto err = SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
	  std::cout << err.getStackTrace() << std::endl;
	}
      }
    }

    if (have_errors) {
      std::ostringstream msg;
      msg << "Failed to process " << have_errors << " store query configuration files";
      auto err = SmartMet::Spine::Exception::Trace(BCP, msg.str());
      if (initial_update && !shutdown_requested) {
	throw err;
      } else {
	std::cout << err.getStackTrace() << std::endl;
      }
    }

    std::unique_lock<std::mutex> lock(mutex2);
    cond.notify_all();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string> bw::StoredQueryMap::get_handler_names() const
{
  try {
    std::vector<std::string> result;
    boost::shared_lock<boost::shared_mutex> lock(mutex);
    for (const auto& item : handler_map) {
      result.push_back(item.first);
    }
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::on_config_error(Fmi::DirectoryMonitor::Watcher watcher,
					 const boost::filesystem::path& path,
					 const boost::regex& pattern,
					 const std::string& message)
{
  (void)watcher;
  (void)path;
  (void)pattern;
  (void)message;
}

boost::shared_ptr<const bw::StoredQueryHandlerBase>
bw::StoredQueryMap::get_handler_by_file_name(const std::string& config_file_name) const
{
  try {
    boost::shared_ptr<const StoredQueryHandlerBase> result;
    boost::shared_lock<boost::shared_mutex> lock(mutex);
    for (auto it2 = handler_map.begin(); not result and it2 != handler_map.end(); ++it2) {
      if (it2->second->get_config()->get_file_name() == config_file_name) {
	result = it2->second;
      }
    }
    return result;
  }
  catch (...) {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<const bw::StoredQueryConfig>
bw::StoredQueryMap::get_query_config_by_file_name(const std::string& config_file_name) const
{
  try {
    boost::shared_ptr<const StoredQueryConfig> result;
    auto handler = get_handler_by_file_name(config_file_name);
    if (handler) {
      result = handler->get_config();
    }
    return result;
  }  catch (...) {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::handle_query_remove(const std::string& config_file_name)
{
  try {
    auto config = get_query_config_by_file_name(config_file_name);
    duplicate.erase(config_file_name);
    if (config) {
      std::ostringstream msg;
      msg << SmartMet::Spine::log_time_str() << ": [WFS] [INFO] Removing storedquery_id='"
	  << config->get_query_id() << "' (File '" << config_file_name << "' deleted)\n";
      std::cout << msg.str() << std::flush;
      boost::unique_lock<boost::shared_mutex> lock(mutex);
      handler_map.erase(Fmi::ascii_tolower_copy(config->get_query_id()));
    }
  } catch (...) {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace {
  void notify_duplicate(const bw::StoredQueryConfig& new_config, const bw::StoredQueryConfig& found_config)
  {
    std::ostringstream msg;
    msg << SmartMet::Spine::log_time_str() << ": [WFS] [WARNING] Duplicate storedquery_id '"
	<< new_config.get_query_id() << "' from file '" << new_config.get_file_name() << "' is ignored."
	<< " Earlier found in '" << found_config.get_file_name() << "'\n";
    std::cout << msg.str() << std::flush;
  }
}

bool bw::StoredQueryMap::should_be_ignored(const StoredQueryConfig& config) const
{
  const bool enable_test = plugin_impl.get_config().getEnableTestQueries();
  const bool enable_demo = plugin_impl.get_config().getEnableDemoQueries();
  return
    config.is_disabled()
    or (not enable_test and config.is_test())
    or (not enable_demo and config.is_demo());
}

boost::optional<std::string>
bw::StoredQueryMap::get_ignore_reason(const StoredQueryConfig& config) const
{
  boost::optional<std::string> reason;
  const bool enable_demo = plugin_impl.get_config().getEnableDemoQueries();
  if (config.is_disabled()) {
    reason = std::string("Disabled stored query");
  } else if (not enable_demo and config.is_demo()) {
    reason = std::string("Disabled DEMO stored query");
  }
  return reason;
}

void bw::StoredQueryMap::handle_query_add(const std::string& config_file_name,
					  const boost::filesystem::path& template_dir,
					  bool initial_update,
					  bool silent_duplicate)
{
  try {
    const int debug_level = plugin_impl.get_debug_level();
    const bool verbose = not initial_update or debug_level > 0;
    StoredQueryConfig::Ptr sqh_config(new StoredQueryConfig(config_file_name, &plugin_impl.get_config()));

    auto prev_handler = get_handler_by_name_nothrow(sqh_config->get_query_id());
    if (prev_handler) {
      if (not silent_duplicate) {
	notify_duplicate(*sqh_config, *prev_handler->get_config());
      }
      duplicate.insert(config_file_name);
    } else {
      duplicate.erase(config_file_name);
      if (should_be_ignored(*sqh_config)) {
	handle_query_ignore(*sqh_config, initial_update);
      } else {
	if (verbose) {
	  std::ostringstream msg;
	  msg << SmartMet::Spine::log_time_str() << ": [WFS] Adding stored query: id='"
	      << sqh_config->get_query_id() << "' config='" << config_file_name << "'\n";
	  std::cout << msg.str() << std::flush;
	}

	enqueue_query_add(sqh_config, template_dir, initial_update);
      }
    }
  } catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::handle_query_modify(const std::string& config_file_name,
					     const boost::filesystem::path& template_dir)
{
  try {
    StoredQueryConfig::Ptr sqh_config(new StoredQueryConfig(config_file_name, &plugin_impl.get_config()));
    const std::string id = sqh_config->get_query_id();

    if (should_be_ignored(*sqh_config)) {
      duplicate.erase(config_file_name);
      handle_query_ignore(*sqh_config, false);
    } else {
      const auto id = sqh_config->get_query_id();
      auto ph1 = get_handler_by_file_name(config_file_name);
      auto ph2 = get_handler_by_name_nothrow(id);
      if (ph1) {
	const auto id2 = ph1->get_config()->get_query_id();
	if (ph2 and (ph1 != ph2)) {
	  request_reload("");
	} else {
	  std::ostringstream msg;
	  msg << SmartMet::Spine::log_time_str() << ": [WFS] [INFO] Adding stored query: id='"
	      << id << "' config='" << config_file_name << "'\n";
	  std::cout << msg.str() << std::flush;
	  enqueue_query_add(sqh_config, template_dir, false);
	  if (ph2 != ph1) {
	    msg.str("");
	    msg << SmartMet::Spine::log_time_str() << ": [WFS] [INFO] Removing stored query: id='" << id2 << "'\n";
	    std::cout << msg.str() << std::flush;
	    boost::unique_lock<boost::shared_mutex> lock(mutex);
	    handler_map.erase(id2);
	  }
	}
      } else {
	// May happen if previous configuration had demo or test setting enabled.
	if (ph2) {
	  // Should add: storedquery_id already present
	  request_reload("");
	} else {
	  // Should add: storedquery_id not in use
	  std::ostringstream msg;
	  msg << SmartMet::Spine::log_time_str() << ": [WFS] [INFO] Adding stored query: id='"
	      << id << "' config='" << config_file_name << "'\n";
	  std::cout << msg.str() << std::flush;
	  enqueue_query_add(sqh_config, template_dir, false);
	}
      }
    }
  } catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::handle_query_ignore(const StoredQueryConfig& sqh_config, bool initial_update)
{
  try {
    const int debug_level = plugin_impl.get_debug_level();
    const auto reason = get_ignore_reason(sqh_config);
    const auto id = sqh_config.get_query_id();
    auto prev_handler = get_handler_by_name_nothrow(sqh_config.get_query_id());
    if (prev_handler) {
      if (sqh_config.get_file_name() == prev_handler->get_config()->get_file_name()) {
	std::ostringstream msg;
	msg << SmartMet::Spine::log_time_str() << ": [WFS] ";
	if (reason) {
	  msg << '[' << *reason << "] ";
	} else {
	  msg << "[Disabled stored query] ";
	}
	msg << "Removing previous handler for " << sqh_config.get_query_id() << '\n';
	std::cout << msg.str() << std::flush;

	boost::unique_lock<boost::shared_mutex> lock(mutex);
	handler_map.erase(Fmi::ascii_tolower_copy(id));
      } else {
	// ID changed since last update: request reload
	request_reload("");
      }
    } else {
      if ((not initial_update or (debug_level > 0)) and reason) {
	std::ostringstream msg;
	msg << SmartMet::Spine::log_time_str() << ": [WFS] [" << *reason << ']'
	    << " id='" << sqh_config.get_query_id() << "' config='" << sqh_config.get_file_name() << "'\n";
	std::cout << msg.str() << std::flush;
      }
    }
  } catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::request_reload(const std::string& reason)
{
  std::ostringstream msg;
  msg << SmartMet::Spine::log_time_str() << ": [WFS] [INFO] Requiring WFS reload due to config change";
  if (reason != "") {
    msg << " (" << reason << ')';
  }
  std::cout << msg.str() << std::endl;
  reload_required = true;
}

boost::shared_ptr<bw::StoredQueryHandlerBase> bw::StoredQueryMap::get_handler_by_name_nothrow(
    const std::string name) const
{
  try
  {
    boost::shared_ptr<bw::StoredQueryHandlerBase> handler;
    const std::string& lname = Fmi::ascii_tolower_copy(name);
    boost::shared_lock<boost::shared_mutex> lock(mutex);
    auto loc = handler_map.find(lname);
    if (loc != handler_map.end()) {
     handler = loc->second;
    }
    return handler;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::enqueue_query_add(StoredQueryConfig::Ptr sqh_config,
					   const boost::filesystem::path& template_dir,
					   bool initial_update)
{
  if (background_init and initial_update) {
    init_tasks->add(sqh_config->get_query_id(),
		    [this, sqh_config, template_dir]() { add_handler(sqh_config, template_dir); });
  } else {
    add_handler(sqh_config, template_dir);
  }
}

void bw::StoredQueryMap::directory_monitor_thread_proc()
{
  std::cout << SmartMet::Spine::log_time_str()
	    << ": [WFS] SmartMet::Plugin::WFS::StoredQueryMap::directory_monitor_proc started"
	    << std::endl;

  directory_monitor.run();

  std::cout << SmartMet::Spine::log_time_str()
	    << ": [WFS] SmartMet::Plugin::WFS::StoredQueryMap::directory_monitor_proc ended"
	    << std::endl;
}

Json::Value bw::StoredQueryMap::get_constructor_map() const
{
  Json::Value result;
  Json::Value& constructors = result["constructors"];
  Json::Value& templates = result["templates"];
  std::map<std::string, std::set<std::string> > m1, m2;
  boost::shared_lock<boost::shared_mutex> lock(mutex);
  for (const auto& map_item : handler_map) {
    auto sq_conf = map_item.second->get_config();
    if (sq_conf->have_template_fn()) {
      const std::string& c_name = sq_conf->get_constructor_name();
      const std::string& t_fn = sq_conf->get_template_fn();
      const std::vector<std::string>& r_names = sq_conf->get_return_type_names();
      Json::Value& a = constructors[c_name];
      Json::Value& a1 = a["templates"];
      if (m1[c_name].insert(t_fn).second) {
	a1[a1.size()] = t_fn;
      }
      Json::Value& a2 = a["stored_queries"];
      Json::Value& b = a2[a2.size()];
      b["name"] = sq_conf->get_query_id();
      b["template"] = sq_conf->get_template_fn();
      Json::Value& b2 = b["return_types"];
      Json::Value& t1 = templates[t_fn];
      for (const auto& x : r_names) {
	b2[b2.size()] = x;
	if (m2[t_fn].insert(x).second) {
	  t1[t1.size()] = x;
	}
      }
    }
  }
  return result;
}
