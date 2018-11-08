#include "StoredQueryHandlerBase.h"
#include "WfsException.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <macgyver/TypeName.h>
#include <newbase/NFmiPoint.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/Value.h>
#include <sstream>
#include <stdexcept>

namespace ba = boost::algorithm;

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
StoredQueryHandlerBase::StoredQueryHandlerBase(SmartMet::Spine::Reactor* reactor,
                                               boost::shared_ptr<StoredQueryConfig> config,
                                               PluginData& plugin_data,
                                               boost::optional<std::string> template_file_name)
    : SupportsExtraHandlerParams(config),
      reactor(reactor),
      config(config),
      template_file(template_file_name),
      hidden(false),
      plugin_data(plugin_data)
{
  try
  {
    const auto& return_types = config->get_return_type_names();
    hidden = config->get_optional_config_param<bool>("hidden", false);

    if (not hidden)
    {
      BOOST_FOREACH (const auto& tn, return_types)
      {
        plugin_data.get_capabilities().register_feature_use(tn);
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

StoredQueryHandlerBase::~StoredQueryHandlerBase() {}

void StoredQueryHandlerBase::init_handler() {}

std::string StoredQueryHandlerBase::get_query_name() const
{
  try
  {
    return config->get_query_id();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string StoredQueryHandlerBase::get_title(const std::string& language) const
{
  try
  {
    return config->get_title(language);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string> StoredQueryHandlerBase::get_return_types() const
{
  try
  {
    return config->get_return_type_names();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<RequestParameterMap> StoredQueryHandlerBase::process_params(
    const std::string& stored_query_id, boost::shared_ptr<RequestParameterMap> orig_params) const
{
  try
  {
    const int debug_level = get_config()->get_debug_level();

    if (debug_level > 1)
    {
      std::cout << SmartMet::Spine::log_time_str() << ' ' << METHOD_NAME
                << ": ORIG_PARAMS=" << *orig_params << std::endl;
    }

    boost::shared_ptr<RequestParameterMap> result =
        StoredQueryParamRegistry::process_parameters(*orig_params, this);
    result->add("storedquery_id", stored_query_id);

    if (debug_level > 1)
    {
      std::cout << SmartMet::Spine::log_time_str() << ' ' << METHOD_NAME
                << ": PROCESSED_PARAMS=" << *result << std::endl;
    }

    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool StoredQueryHandlerBase::redirect(const StoredQuery& query,
                                      std::string& new_stored_query_id) const
{
  try
  {
    (void)query;
    (void)new_stored_query_id;
    return false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const StoredQueryMap& StoredQueryHandlerBase::get_stored_query_map() const
{
  try
  {
    return plugin_data.get_stored_query_map();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::shared_ptr<Fmi::TemplateFormatter> StoredQueryHandlerBase::get_formatter(
    bool debug_format) const
{
  try
  {
    if (debug_format)
    {
      return plugin_data.get_ctpp_dump_formatter();
    }
    else
    {
      if (!template_file)
        throw Spine::Exception(BCP, "Template formatter not set for stored query!");
      return plugin_data.get_stored_query_formatter(*template_file);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredQueryHandlerBase::format_output(CTPP::CDT hash,
                                           std::ostream& output,
                                           bool debug_format) const
{
  try
  {
    std::ostringstream formatter_log;
    auto formatter = get_formatter(debug_format);
    try
    {
      formatter->process(hash, output, formatter_log);
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Template formatter exception", nullptr);
      exception.addDetail(formatter_log.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PROCESSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredQueryHandlerBase::set_2D_coord(
    boost::shared_ptr<SmartMet::Engine::Gis::CRSRegistry::Transformation> transformation,
    double sx,
    double sy,
    CTPP::CDT& hash)
{
  try
  {
    NFmiPoint p1(sx, sy);
    NFmiPoint p2 = transformation->transform(p1);
    double w1 = std::max(std::fabs(p2.X()), std::fabs(p2.Y()));
    int prec = w1 < 1000 ? 5 : std::max(0, 7 - static_cast<int>(std::floor(log10(w1))));
    {
      char buffer[256];
      std::snprintf(buffer, sizeof(buffer), "%.*f", prec, p2.X());
      hash["x"] = buffer;
      std::snprintf(buffer, sizeof(buffer), "%.*f", prec, p2.Y());
      hash["y"] = buffer;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void StoredQueryHandlerBase::set_2D_coord(
    boost::shared_ptr<SmartMet::Engine::Gis::CRSRegistry::Transformation> transformation,
    const std::string& sx,
    const std::string& sy,
    CTPP::CDT& hash)
{
  try
  {
    set_2D_coord(transformation, std::stod(sx), std::stod(sy), hash);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
