#include "SupportsExtraHandlerParams.h"
#include "ArrayParameterTemplate.h"
#include "ScalarParameterTemplate.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/StringConversion.h>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <spine/Value.h>
#include <sstream>

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;

using SmartMet::Spine::Value;

bw::SupportsExtraHandlerParams::SupportsExtraHandlerParams(
    boost::shared_ptr<StoredQueryConfig> config, bool mandatory, const char* path)
    : path(path)
    , handler_params()
    , config(config)
{
  try
  {
    // Leave empty if both are true
    // - presence is not mandatory
    // - requested libconfig entry is not found
    if (not mandatory and not config->get_config().exists(path))
    {
      return;
    }

    auto& c_handler_params = config->get_mandatory_config_param<libconfig::Setting&>(path);
    config->assert_is_list(c_handler_params);

    boost::shared_ptr<ParameterTemplateBase> param_def;

    const int N = c_handler_params.getLength();
    for (int i = 0; i < N; i++)
    {
      auto& param_desc = c_handler_params[i];
      const std::string name = config->get_mandatory_config_param<std::string>(param_desc, "name");
      libconfig::Setting& def =
          config->get_mandatory_config_param<libconfig::Setting&>(param_desc, "def");
      bool reg = config->get_optional_config_param<bool>(param_desc, "register", false);
      const auto item_path = param_desc.getPath();
      if (def.isArray())
      {
        boost::shared_ptr<ArrayParameterTemplate> array_param_def;
        static const int DEF_MAX_SIZE = std::numeric_limits<int>::max();
        const int min_size = config->get_optional_config_param<int>(param_desc, "min_size", 0);
        const int max_size =
            config->get_optional_config_param<int>(param_desc, "max_size", DEF_MAX_SIZE);
        array_param_def.reset(
            new ArrayParameterTemplate(*config, item_path, "def", min_size, max_size));
        if (reg)
          register_array_param(name, array_param_def, min_size, max_size);
        param_def = array_param_def;
      }
      else if (def.isScalar())
      {
        boost::shared_ptr<ScalarParameterTemplate> scalar_param_def;
        scalar_param_def.reset(new ScalarParameterTemplate(*config, item_path, "def"));
        if (reg)
          register_scalar_param(name, scalar_param_def, false);
        param_def = scalar_param_def;
      }
      else
      {
        std::ostringstream msg;
        msg << "Incorrect parameter definition: ";
        config->dump_setting(msg, def, 16);
        SmartMet::Spine::Exception exception(BCP, msg.str());
        exception.addDetail("Scalar or array expected.");
        throw exception;
      }

      assert(param_def);

      if (not handler_params.insert(std::make_pair(Fmi::ascii_tolower_copy(name), param_def))
                  .second)
      {
        throw SmartMet::Spine::Exception(
            BCP, "Duplicate stored query handler parameter '" + name + "'!");
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::SupportsExtraHandlerParams::~SupportsExtraHandlerParams() {}

const bw::ParameterTemplateBase& bw::SupportsExtraHandlerParams::get_param(
    const std::string& name) const
{
  try
  {
    auto it = handler_params.find(Fmi::ascii_tolower_copy(name));
    if (it == handler_params.end())
    {
      std::ostringstream msg;
      if (handler_params.empty())
      {
        msg << " No parameters defined at configuaration path '" << path << "'.";
      }
      else
      {
        char sep = ' ';
        msg << " Available:";
        BOOST_FOREACH (const auto& item, handler_params)
        {
          msg << sep << '\'' << item.first << '\'';
          sep = ',';
        }
      }
      SmartMet::Spine::Exception exception(BCP, "Handler parameter '" + name + "' not found.");
      exception.addDetail(msg.str());
      throw exception;
    }
    else
    {
      return *it->second;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::SupportsExtraHandlerParams::dump_named_params(const bw::RequestParameterMap& req_param_map,
                                                       CTPP::CDT& hash) const
{
  try
  {
    BOOST_FOREACH (const auto& map_item, handler_params)
    {
      const std::string& name = map_item.first;
      bw::ParameterTemplateBase& param_template = *map_item.second;
      boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> > value;
      auto found = param_template.get_value(value, req_param_map.get_map(), this, false);
      if (found != false)
      {
        if (value.which() == 0)
        {
          hash[name][0] = boost::get<SmartMet::Spine::Value>(value).to_string();
        }
        else if (value.which() == 1)
        {
          const auto& value_vect = boost::get<std::vector<SmartMet::Spine::Value> >(value);
          for (std::size_t i = 0; i < value_vect.size(); i++)
          {
            hash[name][i] = value_vect[i].to_string();
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

/**

@page WFS_SQ_NAMED_PARAMS Generic supports of extra named stored query parameters

The C++ class SmartMet::Plugin::WFS::SupportsExtraHandlerParams implements generic support of
additional
named
parameters of stored query handler. Developer must derive stored query handler also from
SmartMet::Plugin::WFS::SupportsExtraHandlerParams to add named handler parameter support.

<b>Note that virtual inheritance MUST be used</b> to have exactly one instance of this class in the
object.

Deriving stored query handler class from SmartMet::Plugin::WFS::SupportsExtraHandlerParams adds
support of mandatory configuration parameter which describes named query handler parameters. The
configuration entry name may be different for different stored query handlers so it is mentioned
in stored query handlers documentation

The value of this entry must be a list of stored query
@ref WFS_SQ_NAMED_PARAM_DEF "named parameter descriptions".

*/

///////////////////////////////////////////////////////////////////////////////////////

/**

@page WFS_SQ_NAMED_PARAM_DEF The stored query named parameter definition

<table>

<tr>
<th>Parameter</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
<td>name</td>
<td>string</td>
<td>mandatory</td>
<td>The name of the query handler parameter</td>
</tr>

<tr>
<td>def</td>
<td>parameter definition</td>
<td>mandatory</td>
<td>The scalar value for scalar parameter definition or the array for array parameter. The
    value of this entru must be either @ref WFS_CFG_SCALAR_PARAM_TMPL "cfgScalarParameterTemplate"
    or @ref WFS_CFG_ARRAY_PARAM_TMPL "cfgArrayParameterTemplate"</td>
</tr>

<tr>
<td>register</td>
<td>boolean</td>
<td>optional (default @b false)</td>
<td>Specifies whether to register the parameter for mapping to stored query parameter</td>
</tr>

<tr>
<td>min_size</td>
<td>integer</td>
<td>optional (array parameters only, default 0)</td>
<td>Defines minimal allowed size of an array for this parameter</td>
</tr>

<tr>
<td>max_size</td>
<td>integer</td>
<td>optional (array parameters only)</td>
<td>Defines maximal allowed size of an array for this parameter</td>
</tr>

</table>

*/
