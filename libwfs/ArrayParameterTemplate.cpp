#include "ArrayParameterTemplate.h"
#include <spine/Exception.h>
#include <algorithm>
#include <typeinfo>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
ArrayParameterTemplate::ArrayParameterTemplate(StoredQueryConfig& config,
                                               const std::string& config_path,
                                               int min_size,
                                               int max_size)
    : ParameterTemplateBase(config, HANDLER_PARAM_NAME, config_path),
      min_size(min_size),
      max_size(max_size)
{
  try
  {
    try
    {
      init();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ArrayParameterTemplate::ArrayParameterTemplate(StoredQueryConfig& config,
                                               const std::string& base_path,
                                               const std::string& config_path,
                                               int min_size,
                                               int max_size)
    : ParameterTemplateBase(config, base_path, config_path), min_size(min_size), max_size(max_size)
{
  try
  {
    try
    {
      init();
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

ArrayParameterTemplate::~ArrayParameterTemplate() {}

std::vector<SmartMet::Spine::Value> ArrayParameterTemplate::get_value(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> > result;
    get_value(result, req_param_map, extra_params, true);
    return boost::get<std::vector<SmartMet::Spine::Value> >(result);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::tribool ArrayParameterTemplate::get_value(
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >& result,
    const RequestParameterMap& req_param_map,
    const SupportsExtraHandlerParams* extra_params,
    bool strict) const
{
  try
  {
    bool some_resolved = false;
    bool all_resolved = true;
    // bool check_lower_limit = true;
    std::vector<SmartMet::Spine::Value> tmp_result;

    BOOST_FOREACH (auto& item, items)
    {
      bool found = true;
      boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> > item_value;
      if (strict)
      {
        item_value = item.get_value(req_param_map, extra_params, true);
      }
      else
      {
        found = item.get_value(item_value, req_param_map, extra_params, true);
      }

      if (found)
      {
        if (item_value.which() == 0)
        {
          tmp_result.push_back(boost::get<SmartMet::Spine::Value>(item_value));
        }
        else if (item_value.which() == 1)
        {
          auto& tmp = boost::get<std::vector<SmartMet::Spine::Value> >(item_value);
          std::copy(tmp.begin(), tmp.end(), std::back_inserter(tmp_result));
        }
        else
        {
          // Not supposed to happen
          assert(0);
        }
        some_resolved = true;
      }
      else
      {
        all_resolved = false;
      }
    }

    if ((int)tmp_result.size() > max_size)
    {
      std::ostringstream msg;
      msg << "Result array size " << tmp_result.size() << " exceeds upper limit " << max_size
          << " of array size";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    if ((int)tmp_result.size() < min_size)
    {
      std::ostringstream msg;
      msg << "Result array size " << tmp_result.size() << " is smaller than lower limit "
          << min_size << " of array size";
      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }

    result = tmp_result;

    if (all_resolved)
    {
      return true;
    }
    else if (some_resolved)
    {
      return boost::indeterminate;
    }
    else
    {
      return false;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<int64_t> ArrayParameterTemplate::get_int_array(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      std::vector<SmartMet::Spine::Value> tmp = get_value(req_param_map, extra_params);
      std::vector<int64_t> result;
      std::transform(tmp.begin(),
                     tmp.end(),
                     std::back_inserter(result),
                     boost::bind(&SmartMet::Spine::Value::get_int, ::_1));
      return result;
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<uint64_t> ArrayParameterTemplate::get_uint_array(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      std::vector<SmartMet::Spine::Value> tmp = get_value(req_param_map, extra_params);
      std::vector<uint64_t> result;
      std::transform(tmp.begin(),
                     tmp.end(),
                     std::back_inserter(result),
                     boost::bind(&SmartMet::Spine::Value::get_uint, ::_1));
      return result;
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<double> ArrayParameterTemplate::get_double_array(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      std::vector<SmartMet::Spine::Value> tmp = get_value(req_param_map, extra_params);
      std::vector<double> result;
      std::transform(tmp.begin(),
                     tmp.end(),
                     std::back_inserter(result),
                     boost::bind(&SmartMet::Spine::Value::get_double, ::_1));
      return result;
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<std::string> ArrayParameterTemplate::get_string_array(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      std::vector<SmartMet::Spine::Value> tmp = get_value(req_param_map, extra_params);
      std::vector<std::string> result;
      std::transform(tmp.begin(),
                     tmp.end(),
                     std::back_inserter(result),
                     boost::bind(&SmartMet::Spine::Value::get_string, ::_1));
      return result;
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<boost::posix_time::ptime> ArrayParameterTemplate::get_ptime_array(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      std::vector<SmartMet::Spine::Value> tmp = get_value(req_param_map, extra_params);
      std::vector<boost::posix_time::ptime> result;
      std::transform(tmp.begin(),
                     tmp.end(),
                     std::back_inserter(result),
                     boost::bind(&SmartMet::Spine::Value::get_ptime, ::_1, true));
      return result;
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Point> ArrayParameterTemplate::get_point_array(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      std::vector<SmartMet::Spine::Value> tmp = get_value(req_param_map, extra_params);
      std::vector<SmartMet::Spine::Point> result;
      std::transform(tmp.begin(),
                     tmp.end(),
                     std::back_inserter(result),
                     boost::bind(&SmartMet::Spine::Value::get_point, ::_1));
      return result;
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::BoundingBox> ArrayParameterTemplate::get_bbox_array(
    const RequestParameterMap& req_param_map, const SupportsExtraHandlerParams* extra_params) const
{
  try
  {
    try
    {
      std::vector<SmartMet::Spine::Value> tmp = get_value(req_param_map, extra_params);
      std::vector<SmartMet::Spine::BoundingBox> result;
      std::transform(tmp.begin(),
                     tmp.end(),
                     std::back_inserter(result),
                     boost::bind(&SmartMet::Spine::Value::get_bbox, ::_1));
      return result;
    }
    catch (...)
    {
      handle_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void ArrayParameterTemplate::init()
{
  try
  {
    std::string base_prefix = get_base_path();
    if (base_prefix != "")
      base_prefix += ".";

    libconfig::Setting* setting_root = get_setting_root();
    if (not setting_root->isArray())
    {
      std::ostringstream msg;
      msg << "The configuration parameter '" << base_prefix << get_config_path()
          << "' is required to be an array";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
    // One does not know actual size of an array here
    else if ((setting_root->getLength() == 0) and (min_size > 0))
    {
      std::ostringstream msg;
      msg << "The length " << setting_root->getLength() << " of configuration parameter '"
          << base_prefix << get_config_path() << "' is out of allowed range " << min_size << ".."
          << max_size;
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
    else
    {
      bool have_weak_ref = false;
      int calc_min_size = 0;
      int calc_max_size = 0;
      int len = setting_root->getLength();
      for (int i = 0; i < len; i++)
      {
        ParameterTemplateItem item;
        libconfig::Setting& setting = (*setting_root)[i];
        SmartMet::Spine::Value item_def = SmartMet::Spine::Value::from_config(setting);
        item.parse(item_def);

        have_weak_ref |= item.weak;

        // If the template item is a reference to WFS request parameter
        // then verify that it is correct. Copying both scalar value and
        // entire array are permitted in in this case
        if (not item.weak and item.param_ref)
        {
          const StoredQueryConfig::ParamDesc& param = get_param_desc(*item.param_ref);

          if (param.isArray())
          {
            if (item.param_ind && *item.param_ind >= param.param_def.getMaxSize())
            {
              // The requested index is above max. possible size of parameter array
              std::ostringstream msg;
              msg << "The array index " << *item.param_ind << " is out of the range 0.."
                  << param.param_def.getMaxSize();
              throw SmartMet::Spine::Exception(BCP, msg.str());
            }
          }

          if (item.param_ind)
          {
            calc_min_size++;
            calc_max_size++;
          }
          else if (param.isArray() /* and not item.param_ind */)
          {
            // Minimal size check is skipped later if parameter
            // template refers to entire optional request array parameter
            calc_min_size += param.param_def.getMinSize();
            calc_max_size += param.param_def.getMaxSize();
          }
          else
          {
            calc_min_size++;
            calc_max_size++;
          }
        }
        else if (item.plain_text)
        {
          calc_min_size++;
          calc_max_size++;
        }

        items.push_back(item);
      }

      if (not have_weak_ref)
      {
        if (calc_min_size > max_size)
        {
          std::ostringstream msg;
          msg << "CONFIGURATION ERROR: calculated minimal"
              << " array size " << calc_min_size << " exceeds upper limit " << max_size
              << " of array parameter for config path '" << base_prefix << get_config_path() << "'";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }

        if (calc_max_size < min_size)
        {
          std::ostringstream msg;
          msg << "CONFIGURATION ERROR: calculated maximal"
              << " array size " << calc_max_size << " is lower than lower limit " << min_size
              << " of array parameter for config path '" << base_prefix << get_config_path() << "'";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }
      }
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

/**

@page WFS_CFG_ARRAY_PARAM_TMPL Stored query handler array parameter template

@section WFS_CFG_ARRAY_PARAM_TMPL_INTRO Introduction

Stored query handler array parameter template describes how to get values for
stored query handler array parameter. Support of array parameter template
is implemented in C++ class SmartMet::Plugin::WFS::ArrayParameterTemplate .

The value of configuration entry which describes stored query handler array
parameter template is an array of @ref WFS_CFG_PARAM_TMPL_ITEM "cfgParameterTemplateItem".
Note that it is not permitted to mix different types in libconfig array. As result one
may need to use strings for values as conversion from strings to aother parameter types
is supported

@section WFS_CFG_ARRAY_PARAM_TMPL_EXAMPLES Examples

Let us assumen that the following stored query array parameters are defined:
@verbatim
parameters:
(
{
        name = "parameters";
        title = { eng = "Parameters to return"; fin = "Meteorologiset parametrit"; };
        abstract = { eng = "Comma separated list of meteorological parameters to return."; fin =
"Meteorologiset paraemtrit pilkulla erotettuna.";};
        xmlType = "gml:NameList";
        type = "string[1..99]";
        minOccurs = 0;
        maxOccurs = 999;
},

{
    name = "foo";
    title = { eng = "Parameter foo"; };
    abstract = { eng = "Parameter foo"; }
    xmlType = "doubleList";
    type = "double[4]";
    minOccurs = 1;
    naxOccurs = 1;
}
);
@endverbatim

Additionally let us assume that following extra named parameter are defined:

@verbatim
handler_params = (
    {
        name = "defaultMeteoParam";
        def = ["t2m","ws_10min","wg_10min",
"wd_10min","rh","td","r_1h","ri_10min","snow_aws","p_sea","vis"];
    }
);
@endverbatim

Several examples of mappping stored query handler parameters are provided in the table below:

<table border="1">

<tr>
<th>Mapping</th>
<th>Description</th>
</tr>

<tr>
<td>
@verbatim
meteoParam = ["${parameters}"];
@endverbatim
</td>
<td>
Unconditionally map stored query handler parameter @b meteoParam to stored query parameter @b
parameters. No default values
are provided.
</td>
</tr>

<tr>
<td>
@verbatim
meteoParam = ["${parameters > defaultMeteoParam}"];
@endverbatim
</td>
<td>
Pap stored query handler parameter @b meteoParam to stored query parameter @b parameters. Use the
value from named parameter
@b defaultMeteoParam as the default
</td>
</tr>

<tr>
<td>
@verbatim
meteoParam = ["longitude", "latitude", "${parameters > defaultMeteoParam}"];
@endverbatim
</td>
<td>
Pap stored query handler parameter @b meteoParam to stored query parameter @b parameters. Use the
value from named parameter
@b defaultMeteoParam as the default. Additionally always insert meteo parameters @b longitude and @b
latitude at
the start of the array parameter
</td>
</tr>

<tr>
<td>
@verbatim
paramFoo = ["${foo[3]}", "${foo[2]}", "${foo[1]}", "${foo[0]}"];
@endverbatim
</td>
<td>
Maps stored query handler parameter @b paramFoo to stored query parameter @b foo reordering array
members.
Note that there is no implemented way to do the same with an arrya of unknwon before length
</td>
</tr>

</table>

 */
