#include "ParameterTemplateItem.h"
#include "RequestParameterMap.h"
#include "SupportsExtraHandlerParams.h"
#include "WfsException.h"
#include <boost/algorithm/string.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/spirit/include/qi.hpp>
#include <macgyver/Exception.h>
#include <spine/Value.h>
#include <sstream>

namespace
{
void optional_deprecate_notice(const std::string& item_def)
{
  try
  {
    std::cerr << "WARNING: bw::ParameterTemplateItem::parse: deprecated optional specification"
              << " '?' in '" << item_def << "' is ignored" << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
void ParameterTemplateItem::parse(const SmartMet::Spine::Value& item_def, bool allow_absent)
{
  try
  {
    if (item_def.type() == typeid(std::string))
    {
      parse(item_def.get_string(), allow_absent);
    }
    else
    {
      absent = false;
      weak = false;
      plain_text.reset(new SmartMet::Spine::Value(item_def));
      param_ref = default_value = boost::optional<std::string>();
      param_ind = boost::optional<std::size_t>();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ParameterTemplateItem::parse(const std::string& item_def, bool allow_absent)
{
  try
  {
    namespace qi = boost::spirit::qi;
    namespace ns = boost::spirit::standard;
    namespace ba = boost::algorithm;
    namespace bl = boost::lambda;

    typedef qi::rule<std::string::const_iterator, std::string(), ns::space_type> qi_rule;

    this->plain_text.reset();
    this->param_ref = this->default_value = boost::optional<std::string>();
    this->param_ind = boost::optional<std::size_t>();

    bool weak = false;
    std::string param_ref;
    boost::optional<std::size_t> param_ind;
    boost::optional<std::string> default_value;
    boost::optional<std::string> redirect_name;

    qi_rule name_p = qi::lexeme[+(ns::alnum | qi::char_('_'))];
    qi_rule ind_p = qi::char_('[') >> qi::uint_[bl::var(param_ind) = bl::_1] >> qi::char_(']');
    qi_rule dflt_p = qi::lexeme[*(qi::char_ - qi::char_('}'))];
    qi_rule cond_ind_p = (ind_p | qi::eps[bl::var(param_ind) = boost::optional<std::size_t>()]);
    qi_rule ref_type_p =
        (qi::char_('$')[bl::var(weak) = false] | qi::char_('%')[bl::var(weak) = true]);

    qi_rule cond_dflt_p = ((qi::char_(':') >> dflt_p[bl::var(default_value) = bl::_1]) |
                           (qi::char_('>') >> name_p[bl::var(redirect_name) = bl::_1]) |
                           qi::eps[bl::var(default_value) = boost::optional<std::string>(),
                                   bl::var(redirect_name) = boost::optional<std::string>()]);

    qi_rule absent_p = ns::string("${") >> ns::string("}") >> qi::eoi;

    qi_rule gen_ref_p =
        ref_type_p >> qi::char_('{') >> qi::lexeme[*(ns::char_ - ns::char_('}'))] >> ns::char_('}');

    qi_rule ref_p = ref_type_p >> qi::char_('{') >>
                    (qi::char_('?')[boost::bind(&optional_deprecate_notice, item_def)] | qi::eps) >>
                    name_p[bl::var(param_ref) = bl::_1] >> cond_ind_p >> cond_dflt_p >>
                    qi::char_('}') >> qi::eoi;

    const std::string src = ba::trim_copy(item_def);
    if (qi::phrase_parse(src.begin(), src.end(), ref_p, ns::space))
    {
      this->reset();
      this->absent = false;
      this->weak = weak;
      this->param_ref = param_ref;
      this->param_ind = param_ind;
      this->default_value = default_value;
      this->redirect_name = redirect_name;
    }
    else if (allow_absent and qi::phrase_parse(src.begin(), src.end(), absent_p, ns::space))
    {
      this->reset();
      this->absent = true;
    }
    else if (qi::phrase_parse(src.begin(), src.end(), gen_ref_p, ns::space))
    {
      throw Fmi::Exception(BCP, "Failed to parse parameter template '" + src + "'!");
    }
    else
    {
      this->reset();
      this->plain_text.reset(new SmartMet::Spine::Value(item_def));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ParameterTemplateItem::reset()
{
  try
  {
    absent = false;
    weak = false;
    plain_text.reset();
    param_ref = boost::optional<std::string>();
    param_ind = boost::optional<std::size_t>();
    default_value = boost::optional<std::string>();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >
ParameterTemplateItem::get_value(const RequestParameterMap& req_param_map,
                                 const SupportsExtraHandlerParams* extra_params,
                                 bool allow_array) const
{
  try
  {
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> > result;
    if (get_value(result, req_param_map, extra_params, allow_array))
    {
      return result;
    }
    else if (absent)
    {
      std::ostringstream msg;
      msg << "CONFIGURATION ERROR: absent parameter specification not"
          << " supported for current parameter!";
      throw Fmi::Exception(BCP, msg.str());
    }
    else
    {
      // Fail with parse error when both
      //   1) the parameter is not found in WFS request
      //   2) no default value is provided
      Fmi::Exception exception(
          BCP, "Mandatory parameter '" + *param_ref + "' not provided in WFS request!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      throw exception;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool ParameterTemplateItem::get_value(
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >& result,
    const RequestParameterMap& req_param_map,
    const SupportsExtraHandlerParams* extra_params,
    bool allow_array) const
{
  try
  {
    if (absent)
    {
      return false;
    }
    else if (plain_text)
    {
      // If an exact string provided in the configuration then return it.
      // Note that casting to actual type is not done here
      result = SmartMet::Spine::Value(*plain_text);
      return true;
    }
    else if (param_ref)
    {
      const std::string& ref_name = *param_ref;
      auto value_vect = req_param_map.get_values(ref_name);
      if (value_vect.empty())
      {
        if (default_value)
        {
          // Return the default value as the string when both:
          //   1) the parameter is not found in WFS request
          //   2) the default value is provided
          result = SmartMet::Spine::Value(*default_value);
          return true;
        }
        else if (redirect_name)
        {
          return handle_redirection(result, req_param_map, extra_params, *redirect_name);
        }
        else
        {
          if (allow_array)
          {
            result = value_vect;
            return true;
          }
          else
          {
            return false;
          }
        }
      }
      else
      {
        if (param_ind)
        {
          if (*param_ind >= value_vect.size())
          {
            // Provided index is out of range
            if (default_value)
            {
              // Provided index is out of range and default value present
              result = SmartMet::Spine::Value(*default_value);
              return true;
            }
            else if (redirect_name)
            {
              // Provided index is out of range and redirection present
              return handle_redirection(result, req_param_map, extra_params, *redirect_name);
            }
            else
            {
              // Fail with parse error when both
              //   1) the index of parameter is provided
              //   2) it is out of range (for actually provided array)
              // NOTE: this error should not normally happen and it most
              //       likely indicates configuration problem
              std::ostringstream msg;
              msg << ": The index " << *param_ind << " for parameter '" << *param_ref
                  << "' is out of range 0.." << (value_vect.size() - 1);
              Fmi::Exception exception(BCP, msg.str());
              exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
              throw exception;
            }
          }
          else
          {
            // SmartMet::Spine::Value found in the array: return it
            result = value_vect.at(*param_ind);
            return true;
          }
        }
        else
        {
          if (value_vect.size() == 1)
          {
            // Return single element when only one is present
            result = *value_vect.begin();
            return true;
          }
          else if (allow_array)
          {
            // Return entire array if that is allowed and size
            // of the array is different from 1
            result = value_vect;
            return true;
          }
          else
          {
            // Array found, but it is not permitted to return an array
            // NOTE: this error should not normally happen and it most
            //       likely indicates configuration problem
            Fmi::Exception exception(
                BCP, "Cannot return an array (parameter '" + *param_ref + "')");
            exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
            throw exception;
          }
        }
      }
    }
    else
    {
      // This is not supposed to happen and indicates error in program code
      throw Fmi::Exception(
          BCP, "INTERNAL ERROR: neither of members plain_text and param_ref are set!");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool ParameterTemplateItem::handle_redirection(
    boost::variant<SmartMet::Spine::Value, std::vector<SmartMet::Spine::Value> >& result,
    const RequestParameterMap& req_param_map,
    const SupportsExtraHandlerParams* extra_params,
    const std::string& redirect_name) const
{
  try
  {
    // Return failure if no registered handler parameters available
    if (not extra_params)
    {
      throw Fmi::Exception(BCP, "Parameter redirection required but not defined!");
    }

    const auto& param_desc = extra_params->get_param(redirect_name);

    /* FIXME: should we allow recursive redirection? Currently not allowed. */

    boost::tribool ok = param_desc.get_value(result, req_param_map, nullptr /* extra_params */);
    return (ok ? true : false);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet

/**

@page WFS_CFG_PARAM_TMPL_ITEM cfgParameterTemplateItem

@section WFS_CFG_PARAM_TMPL_ITEM_INTRO Introduction

This configuration type described a single mapping of stored query handler parameters to stored
query parameter
or its element. Parameter template item may provide:
- constant stored query parameter value
- mapping to stored query scalar or array parameter or array parameter member
- provide the default values as constants
- provide the default values as redirect to stored query handler named parameter

@section WFS_CFG_PARAM_TMPL_ITEM_FORMAT Item format

There are several possible formats of parameter template item specification. Table below contains
several
examples of parameter template item, but there are more combinations possible

<table border="1">

<tr>
<td>constant</td>
<td>Uses provided constant for the value of parameter template item</td>
</td>

<tr>
<td>
@verbatim
${}
@endverbatim
</td>
<td>Ommitted query parameter. Note that this cannot be used for stored query parameters which are
queries
    unconditionally</td>
</tr>

<tr>
<td>
@verbatim
${foo}
@endverbatim
</td>
<td>Mapps stored query handler parameter to query parameter @b foo . There is no default value if
the stored
    query parameter @b foo is not provided.</td>
</tr>

<tr>
<td>
@verbatim
${foo : bar}
@endverbatim
</td>
<td>Mapps stored query handler parameter to query parameter @b foo . Use the default value (string)
'bar' if
    stored query parameter @b foo is not provided. Note that it must be possible to convert provided
    default string to actual type of parameter. An error response is generated if such conversion
fails.</td>
</tr>

<tr>
<td>
@verbatim
${foo > bar}
@endverbatim
</td>
<td>Mapps stored query handler parameter to query parameter @b foo . Use as the default value named
parameter
   @b bar if stored query parameter @b foo is not provided.</td>
</td>

<tr>
<td>
@verbatim
${foo[3] > bar}
@endverbatim
</td>
<td>Mapps stored query handler parameter to query array parameter @b foo member with index 3. The
default value
   (named parameter @b bar) is used if either stored query parameter @b foo is not present or index
3 (4th element)
   is out of range.</td>
</td>

</table>

The presence and corectness (type, range check if specified) of parameter is verified when parsing
stored
query request for all examples given in the table above.

It is possible to avoid check at query parsing time by replacing '$' in maping to stored query
parameter with '\%'.
That is intended to be used in cases when stored query handler translates the parameters provided in
query to
different ones.

Note that for named parameter configuration entry @b register with value true must be provided for
mapping to stored
query parameters to work at query parsing time.

 */
