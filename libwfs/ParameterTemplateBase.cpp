#include "ParameterTemplateBase.h"
#include "RequestParameterMap.h"
#include "SupportsExtraHandlerParams.h"
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <sstream>
#include <string>
#include <typeinfo>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
const char* ParameterTemplateBase::HANDLER_PARAM_NAME = "handler_params";

ParameterTemplateBase::ParameterTemplateBase(StoredQueryConfig& config,
                                             const std::string& base_path,
                                             const std::string& config_path)
    : config(config), base_path(base_path), config_path(config_path)
{
}

ParameterTemplateBase::~ParameterTemplateBase() {}

libconfig::Setting* ParameterTemplateBase::get_setting_root()
{
  try
  {
    auto& root = base_path == ""
                     ? config.get_config().getRoot()
                     : config.get_mandatory_config_param<libconfig::Setting&>(base_path);
    return &config.get_mandatory_config_param<libconfig::Setting&>(root, config_path);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const StoredQueryConfig::ParamDesc& ParameterTemplateBase::get_param_desc(
    const std::string& link_name) const
{
  try
  {
    namespace ba = boost::algorithm;

    auto& desc_map = config.get_param_descriptions();
    auto loc = desc_map.find(Fmi::ascii_tolower_copy(link_name));
    if (loc == desc_map.end())
    {
      std::ostringstream msg;
      throw SmartMet::Spine::Exception(
          BCP, "The description of the '" + link_name + "' parameter is not found!");
    }
    else
    {
      loc->second.set_used();
      return loc->second;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void ParameterTemplateBase::handle_exceptions(const std::string& location) const
{
  try
  {
    // FIXME: catch other exceptions if needed

    try
    {
      throw;
    }
    catch (const libconfig::ConfigException& err)
    {
      std::ostringstream msg;
      msg << location << ": error getting data from configuration:"
          << " path='" << config_path << "' exception='" << Fmi::current_exception_type()
          << "' message='" << err.what() << "'";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
    catch (const boost::bad_lexical_cast& err)
    {
      std::ostringstream msg;
      msg << location
          << " [INTERNAL ERROR]: lexical_cast<> failed (configuration error suspected): "
          << err.what();
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
