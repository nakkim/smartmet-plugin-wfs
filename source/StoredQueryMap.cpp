#include "StoredQueryMap.h"
#include "StoredQueryHandlerFactoryDef.h"
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <macgyver/TypeName.h>
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

class SmartMet::Plugin::WFS::StoredQueryMap::Init
{
  static std::size_t MAX_COUNT;

  typedef boost::chrono::duration<long long, boost::micro> microseconds;
  typedef std::list<boost::shared_ptr<boost::thread> >::iterator iterator;

  std::list<boost::shared_ptr<boost::thread> > threads;

 public:
  void add(boost::shared_ptr<boost::thread> thread)
  {
    try
    {
      // Ensure that there are not too many initialization threads
      wait(MAX_COUNT);

      threads.push_back(thread);
    }
    catch (...)
    {
      throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
    }
  }

  void wait(std::size_t max_count)
  {
    try
    {
      iterator it, next;

      while (threads.size() > max_count)
      {
        bool found = false;
        // Join threads are are immediatelly joinable
        for (it = threads.begin(); it != threads.end(); it = next)
        {
          next = it;
          ++next;
          boost::shared_ptr<boost::thread> thread = *it;
          if (thread->try_join_for(microseconds(0)))
          {
            found = true;
            threads.erase(it);
          }
        }

        // No immediatelly joinable threads: try to find some
        // thread that will be joinable soon
        if (not found)
        {
          for (it = threads.begin(); it != threads.end() and not found; it = next)
          {
            next = it;
            ++next;
            boost::shared_ptr<boost::thread> thread = *it;
            if (thread->try_join_for(microseconds(10000)))
            {
              found = true;
              threads.erase(it);
            }
          }
        }
      }
    }
    catch (...)
    {
      throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
    }
  }
};

std::size_t bw::StoredQueryMap::Init::MAX_COUNT = 30;

bw::StoredQueryMap::StoredQueryMap() : background_init(false), init(new Init) {}

bw::StoredQueryMap::~StoredQueryMap() {}

void bw::StoredQueryMap::set_background_init(bool value)
{
  background_init = value;
}

void bw::StoredQueryMap::add_handler(boost::shared_ptr<StoredQueryHandlerBase> handler)
{
  try
  {
    SmartMet::Spine::WriteLock lock(mutex);
    const std::string name = handler->get_query_name();
    std::string lname = Fmi::ascii_tolower_copy(handler->get_query_name());

    auto find_iter = handler_map.find(lname);
    if (find_iter != handler_map.end())
    {
      // Override already defined stored query with the current one
      handler_map.erase(find_iter);

      auto remove_iter = std::remove_if(
          handler_names.begin(), handler_names.end(), boost::bind(&::is_equal, name, _1));
      handler_names.erase(remove_iter, handler_names.end());

      std::ostringstream msg;
      msg << SmartMet::Spine::log_time_str() << ": [WFS] Overriding stored query with name '"
          << name << "'";

      std::cout << msg.str() << std::endl;
    }

    handler_names.push_back(name);

    handler_map.insert(std::make_pair(lname, handler));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::add_handler(SmartMet::Spine::Reactor* theReactor,
                                     const boost::filesystem::path& stored_query_config_file,
                                     const boost::filesystem::path& template_dir,
                                     PluginImpl& plugin_data)
{
  try
  {
    try
    {
      using SmartMet::Spine::log_time_str;

      std::string prefix = "";
      int debug_level = plugin_data.get_debug_level();

      const std::string file_name = stored_query_config_file.string();

      boost::shared_ptr<StoredQueryConfig> sqh_config
	(new StoredQueryConfig(file_name, &plugin_data.get_config()));

      if (sqh_config->is_test())
      {
        if (plugin_data.get_config().getEnableTestQueries())
        {
          prefix = "TEST ";
        }
        else
        {
          // Silently skip test queries if not explicitly enabled
          return;
        }
      }

      if (sqh_config->is_disabled())
      {
        if (debug_level > 0)
        {
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str() << ": [WFS] [Disabled stored query] id='"
              << sqh_config->get_query_id() << "' config='" << file_name << "'\n";
          std::cout << msg.str() << std::flush;
        }
      }
      else if (sqh_config->is_demo() and not plugin_data.get_config().getEnableDemoQueries())
      {
        if (debug_level > 0)
        {
          std::ostringstream msg;
          msg << SmartMet::Spine::log_time_str() << ": [WFS] [Disabled DEMO stored query]"
              << " id='" << sqh_config->get_query_id() << "' config='" << file_name << "'\n";
          std::cout << msg.str() << std::flush;
        }
      }
      else
      {
        if (background_init)
        {
          boost::shared_ptr<boost::thread> thread(
              new boost::thread(boost::bind(&bw::StoredQueryMap::add_handler_thread_proc,
                                            this,
                                            theReactor,
                                            sqh_config,
                                            boost::ref(template_dir),
                                            boost::ref(plugin_data))));
          init->add(thread);
        }
        else
        {
          add_handler(theReactor, sqh_config, template_dir, plugin_data);
        }
      }
    }
    catch (...)
    {
      throw SmartMet::Spine::Exception::Trace(
          BCP, "Error while processing stored query configuration file!")
          .addParameter("Query config file", stored_query_config_file.string());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::StoredQueryMap::read_config_dir(SmartMet::Spine::Reactor* theReactor,
                                         const boost::filesystem::path& config_dir,
                                         const boost::filesystem::path& template_dir,
                                         PluginImpl& plugin_data)
{
  try
  {
    std::queue<boost::shared_ptr<boost::thread> > init_threads;

    if (not fs::exists(config_dir))
    {
      throw SmartMet::Spine::Exception(BCP, "Directory '" + config_dir.string() + "' not found!");
    }

    if (not fs::is_directory(config_dir))
    {
      throw SmartMet::Spine::Exception(BCP, "'" + config_dir.string() + "' is not a directory!");
    }

    for (auto it = fs::directory_iterator(config_dir); it != fs::directory_iterator(); ++it)
    {
      fs::path config_entry = *it;
      const std::string name = config_entry.string();
      const std::string bn = config_entry.filename().string();

      if (fs::is_regular_file(name) and not ba::starts_with(bn, ".") and
          not ba::starts_with(bn, "#") and name.substr(name.length() - 5) == ".conf")
      {
        add_handler(theReactor, name, template_dir, plugin_data);
      }
    }

    if (background_init)
    {
      init->wait(0U);
      std::ostringstream msg;
      msg << SmartMet::Spine::log_time_str()
          << ": [WFS] Stored query background initialization finished\n";
      std::cout << msg.str() << std::flush;
    }
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
    SmartMet::Spine::ReadLock lock(mutex);
    auto loc = handler_map.find(lname);
    if (loc == handler_map.end())
    {
      SmartMet::Spine::Exception exception(BCP, "No handler for '" + name + "' found!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
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
    SmartMet::Spine::ReadLock lock(mutex);
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

void bw::StoredQueryMap::add_handler(SmartMet::Spine::Reactor* theReactor,
                                     boost::shared_ptr<StoredQueryConfig> sqh_config,
                                     const boost::filesystem::path& template_dir,
                                     PluginImpl& plugin_data)
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
          sqh_config->get_constructor_name(), theReactor, sqh_config, plugin_data, sqh_template_fn);

      add_handler(p_handler);

      if (plugin_data.get_debug_level() < 1)
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

void bw::StoredQueryMap::add_handler_thread_proc(SmartMet::Spine::Reactor* theReactor,
                                                 boost::shared_ptr<bw::StoredQueryConfig> config,
                                                 const boost::filesystem::path& template_dir,
                                                 bw::PluginImpl& plugin_data)
{
  try
  {
    try
    {
      add_handler(theReactor, config, template_dir, plugin_data);
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

void bw::StoredQueryMap::update_handlers(Spine::Reactor* theReactor, bw::PluginImpl& plugin_data)
{
  try
  {
    for (const auto& sqName : handler_names)
    {
      std::ostringstream msg;

      if (auto handler = get_handler_by_name(sqName))
      {
        auto config_write_time = handler->get_config()->config_write_time();
        if (handler->get_config()->last_write_time_changed())
        {
          boost::shared_ptr<StoredQueryConfig> sqh_config(
              new StoredQueryConfig(handler->get_config()->get_file_name(), &plugin_data.get_config()));

          if (sqName != sqh_config->get_query_id())
          {
            msg << Spine::log_time_str() << ": [WFS] Stored query with name '" << sqName
                << "' has changed to '" << sqh_config->get_query_id()
                << "'. The name change iqnored! CHANGE THE NAME BACK TO '" << sqName
                << "' OR RESTART THE SERVER!\n";
            std::cerr << msg.str() << std::flush;
            continue;
          }

          char mbstr[100];
          const char* fmt = "%Y-%m-%d %H:%M:%S";
          std::strftime(mbstr, sizeof(mbstr), fmt, std::localtime(&config_write_time));

          msg << Spine::log_time_str() << ": [WFS] Updating stored query with name '" << sqName
              << "'. Last write time of file '" << handler->get_config()->get_file_name()
              << "' is now " << mbstr << ".\n";
          std::cerr << msg.str() << std::flush;

          add_handler(theReactor,
                      boost::filesystem::path(handler->get_config()->get_file_name()),
                      plugin_data.get_config().get_template_directory(),
                      plugin_data);
        }
      }
      else
      {
        msg << Spine::log_time_str() << ": [WFS] [ERROR] Stored query name '" << sqName
            << "' is found from StoredQueryMap but a handler not found for it!\n";
        std::cerr << msg.str() << std::flush;
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "A stored query handler update failed!");
  }
}
