#pragma once

#include "PluginImpl.h"
#include "StandardPresentationParameters.h"
#include "StoredQuery.h"
#include "StoredQueryConfig.h"
#include "StoredQueryHandlerInitBase.h"
#include "StoredQueryMap.h"
#include "StoredQueryParamRegistry.h"
#include "SupportsExtraHandlerParams.h"

#include <spine/CRSRegistry.h>
#include <spine/Reactor.h>
#include <spine/Value.h>
#include <spine/ValueFormatter.h>

#include <macgyver/TemplateFormatter.h>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace Fmi
{
class TemplateFormatter;
}

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class PluginImpl;
class StoredQuery;
class StoredQueryMap;

class StoredQueryHandlerBase : virtual protected SupportsExtraHandlerParams,
                               virtual protected StoredQueryParamRegistry,
                               virtual protected StoredQueryHandlerInitBase
{
  SmartMet::Spine::Reactor* reactor;
  boost::shared_ptr<StoredQueryConfig> config;
  bool hidden;

 protected:
  const PluginImpl& plugin_impl;
  boost::optional<std::string> template_file;  // needed by StoredFlashQueryHandler

 public:
  StoredQueryHandlerBase(SmartMet::Spine::Reactor* reactor,
                         boost::shared_ptr<StoredQueryConfig> config,
                         PluginImpl& plugin_impl,
                         boost::optional<std::string> template_file_name);

  virtual ~StoredQueryHandlerBase();

  void perform_init();

  virtual std::string get_query_name() const;

  virtual std::string get_title(const std::string& language) const;

  virtual std::vector<std::string> get_return_types() const;

  /**
   *   @brief Performs initial processing of stored query parameters
   *
   *   The default action (in base class) is to simply return
   *   original parameters unchanged.
   */
  virtual boost::shared_ptr<RequestParameterMap> process_params(
      const std::string& stored_query_id, boost::shared_ptr<RequestParameterMap> orig_params) const;

  virtual void query(const StoredQuery& query,
                     const std::string& language,
		     const boost::optional<std::string>& hostname,
                     std::ostream& output) const = 0;

  /**
   *   @brief Check whether the request must be redirected to a different
   *          stored request
   *
   *   @param query The StoredQuery object
   *   @param new_stored_query_id The redirected stored query ID (only need to be set
   *          when return value is @c true)
   *   @retval false no redirection (the original request is OK)
   *   @retval true the request must be redirected to a different stored query handler.
   *          In this case method @b must put redirected stored query ID into the second
   *          parameter
   *
   *   The return value is @b false in the base class.
   */
  virtual bool redirect(const StoredQuery& query, std::string& new_stored_query_id) const;

  inline boost::shared_ptr<const StoredQueryConfig> get_config() const { return config; }
  const StoredQueryMap& get_stored_query_map() const;

  inline bool is_hidden() const { return hidden; }
  inline std::set<std::string> get_handler_param_names() const
  {
    return StoredQueryParamRegistry::get_param_names();
  }

 protected:
  virtual void init_handler();

  boost::shared_ptr<Fmi::TemplateFormatter> get_formatter(bool debug_format) const;

  inline SmartMet::Spine::Reactor* get_reactor() const { return reactor; }
  inline const PluginImpl& get_plugin_impl() const { return plugin_impl; }
  void format_output(CTPP::CDT& hash, std::ostream& output, bool debug_format) const;

  static std::pair<std::string, std::string> get_2D_coord(
      boost::shared_ptr<SmartMet::Spine::CRSRegistry::Transformation> transformation,
      double X,
      double Y);

  static void set_2D_coord(
      boost::shared_ptr<SmartMet::Spine::CRSRegistry::Transformation> transformation,
      double X,
      double Y,
      CTPP::CDT& hash);

  static void set_2D_coord(
      boost::shared_ptr<SmartMet::Spine::CRSRegistry::Transformation> transformation,
      const std::string& X,
      const std::string& Y,
      CTPP::CDT& hash);
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
