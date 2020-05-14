#include "UrlTemplateGenerator.h"
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/spirit/include/qi.hpp>
#include <macgyver/TypeName.h>
#include <spine/Exception.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;

namespace
{
void encode(std::ostream& stream, const std::string& text)
{
  try
  {
    // const char *special = "$&+,:/;=?@'\"<>#%{}|\\^~[]";
    const char* special = "$&+/=?@'\"<>#%{}|\\^~[]";
    BOOST_FOREACH (unsigned char ch, text)
    {
      if ((ch <= 0x1F) or (ch >= 0x7F) or strchr(special, ch))
      {
        char tmp[16];
        *tmp = '%';
        *ba::hex(&ch, (&ch) + 1, tmp + 1) = 0;
        stream << tmp;
      }
      else if (ch == ' ')
      {
        stream << '+';
      }
      else
      {
        stream << ch;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

bw::UrlTemplateGenerator::UrlTemplateGenerator(const std::string& url,
                                               const std::vector<std::string>& param_templates)
    : url(url), params()
{
  try
  {
    std::for_each(param_templates.begin(),
                  param_templates.end(),
                  boost::bind(&bw::UrlTemplateGenerator::parse_param_def, this, ::_1));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bw::UrlTemplateGenerator::~UrlTemplateGenerator() {}

std::string bw::UrlTemplateGenerator::generate(
    boost::function1<std::vector<std::string>, std::string> param_getter_cb) const
{
  try
  {
    std::string sep = "";
    std::ostringstream result;
    result << eval_string_param(url, param_getter_cb, false);

    if (params.size() > 0)
    {
      if ((url != "") and (*url.rbegin() != '?'))
      {
        if (strchr(url.c_str(), '?') != nullptr)
        {
          sep = *url.rbegin() != '&' ? "&" : "";
        }
        else
        {
          sep = "?";
        }
      }

      BOOST_FOREACH (const auto param, params)
      {
        if (param.type() == typeid(ParamRef))
        {
          const auto& ref = boost::get<ParamRef>(param);
          const auto value_vect = param_getter_cb(ref.name);
          if (ref.separate_param)
          {
            BOOST_FOREACH (const auto& item, value_vect)
            {
              result << sep;
              encode(result, ref.name);
              result << '=';
              encode(result, item);
              sep = "&";
            }
          }
          else if (not(ref.omit_empty and value_vect.empty()))
          {
            char sep2[2] = {0, 0};
            result << sep;
            encode(result, ref.name);
            result << '=';
            BOOST_FOREACH (const auto& item, value_vect)
            {
              if (std::strchr(item.c_str(), ',') != nullptr)
              {
                SmartMet::Spine::Exception exception(
                    BCP, "List items are not permitted to contain ',' characters!");
                exception.addParameter("Item", item.c_str());
                throw exception;
              }
              result << sep2;
              encode(result, item);
              sep2[0] = ',';
            }
            sep = "&";
          }
        }
        else if (param.type() == typeid(StringParam))
        {
          const auto& p = boost::get<StringParam>(param);
          result << sep;
          encode(result, p.name);
          result << '=';
          encode(result, eval_string_param(p.value, param_getter_cb));
          sep = "&";
        }
      }
    }

    return result.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::UrlTemplateGenerator::parse_param_def(const std::string& str)
{
  try
  {
    namespace qi = boost::spirit::qi;
    namespace ns = boost::spirit::standard;
    namespace ba = boost::algorithm;
    namespace bl = boost::lambda;

    const auto& src = ba::trim_copy(str);
    bool may_skip = false;
    bool separate_param = false;
    bool have_const = false;
    bool have_ref = false;
    std::string name, ref_name, value;

    typedef qi::rule<std::string::const_iterator, std::string()> qi_rule;

    qi_rule name_p = qi::lexeme[+(ns::alnum | qi::char_('_'))];

    qi_rule value_p = qi::lexeme[*qi::char_];

    qi_rule ref_p = qi::char_('$') >> qi::char_('{') >>
                    -((qi::char_('?')[bl::var(may_skip) = true]) |
                      (qi::char_('*')[bl::var(separate_param) = true])) >>
                    name_p[bl::var(ref_name) = bl::_1] >> qi::char_('}');

    qi_rule const_p =
        name_p[bl::var(name) = bl::_1] >> qi::char_('=') >> value_p[bl::var(value) = bl::_1];

    qi_rule parser_p = const_p[bl::var(have_const) = true] | ref_p[bl::var(have_ref) = true];

    if (qi::phrase_parse(src.begin(), src.end(), parser_p, ns::space))
    {
      if (have_const)
      {
        StringParam param;
        param.name = name;
        param.value = value;
        params.push_back(param);
      }
      else if (have_ref)
      {
        ParamRef ref;
        ref.name = ref_name;
        ref.omit_empty = may_skip;
        ref.separate_param = separate_param;
        params.push_back(ref);
      }
      else
      {
        assert(0);
      }
    }
    else
    {
      std::ostringstream msg;
      msg << "Failed to parse parameter definition '" << src << "'!";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::UrlTemplateGenerator::eval_string_param(
    const std::string& param_str,
    boost::function1<std::vector<std::string>, std::string> param_getter_cb,
    bool allow_list)
{
  try
  {
    std::ostringstream result;

    bool fail = false;

    const char* src = param_str.c_str();

    while (*src and not fail)
    {
      if (src[0] == '\\')
      {
        if (not src[1])
        {
          std::ostringstream msg;
          msg << "bw::UrlTemplateGenerator::eval_string_param"
              << ": unexpected end of source string '" << param_str << "'";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }
        result << src[1];
        src += 2;
      }
      else if (src[0] == '$')
      {
        if (src[1] != '{')
        {
          std::ostringstream msg;
          msg << "bw::UrlTemplateGenerator::eval_string_param"
              << ": missing '{' after '$' in '" << param_str << "'";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }

        src += 2;
        const char* name = src;
        while (std::isalnum(*src) or *src == '_')
        {
          src++;
        }

        if (*src != '}')
        {
          std::ostringstream msg;
          msg << "bw::UrlTemplateGenerator::eval_string_param"
              << ": invalid parameter name after '${' or closing '}' missing in '" << param_str
              << "'";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }

        const std::string ref_name(name, src);
        src++;

        const auto values = param_getter_cb(ref_name);
        const int N = values.size();

        if (not allow_list and N > 1)
        {
          std::ostringstream msg;
          msg << "bw::UrlTemplateGenerator::eval_string_param: scalar value expected when"
              << " substituting value of 'ref_name' (got";
          for (int i = 0; i < N; i++)
            msg << " '" << values[i] << "'";
          msg << ")";
          throw SmartMet::Spine::Exception(BCP, msg.str());
        }

        for (int i = 0; i < N; i++)
        {
          if (i > 0)
            result << ',';
          result << values[i];
        }
      }
      else
      {
        result << *src++;
      }
    }

    return result.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/**

@page WFS_CFG_URL_TEMPLATE HTML URL generator template

URL generation from template is implemented in C++ class
SmartMet::Plugin::WFS::UrlTemplateGenerator.

The instructions for URL generation is provided for libconfig configuration group for WFS plugin.
This group consists of the following entries:

<table border="1">

<tr>
<th>Entry name</th>
<th>Type</th>
<th>Use</th>
<th>Description</th>
</tr>

<tr>
  <td>url</td>
  <td>string</td>
  <td>mandatory</td>
  <td>Describes URL to which parameters are appended. Substitutions of stored query handler named
parameter
      values are supported in this string:
      - <b>${name}</b> is being replaced by the value of handler named parameter @b name
      - one can escape symbols '<b>\\</b>' and '<b>$</b>' by prepending them with '<b>\\</b>'
</tr>

<tr>
  <td>params</td>
  <td>array of strings</td>
  <td>mandatory</td>
  <td>Describes URL parameters to add to generated URL. Following formats are supported:
  - <b>${x}</b> - adds parameter <b>x</b>=<i>value_of_x</i>, where <i>value_of_x</i> is the
       value of query named parameter @b x. ',' separated list is used for values when more
       than one value is found
  - <b>${?x}</b> - adds parameter <b>x</b>=<i>value_of_x</i>, where <i>value_of_x</i> is the
       value of query named parameter @b x only is named parameter value is found. ',' separated
       list is used for values when more than one value is found
  - <b>${*x}</b> - adds parameter <b>x</b>=<i>value_of_x</i>, where <i>value_of_x</i> is the
       value of query named parameter @b x only is named parameter value is found. Separate
       parameter is appended for each value of parameter if there are more than one.
  - <b>param_name=param_value</b> - adds parameter with specified name and value. The substitutions
       of named parameters in the value is performed in the same way as for entry @b url
  </td>
</tr>

</table>

This configuration object is being read in
SmartMet::Plugin::WFS::StoredAtomQueryHandlerBase::StoredAtomQueryHandlerBase.

The generated URL together with the parameters is made available to @b CTPP template processor. As
the result
the actual URL value without the parameters can be left out in this configuration group and be
provided
in @b CTPP2 template instead. In that case one must take into account that URL generation
automatically
appends '<b>?</b>' before the first parameter when none are found before (otherwise '<b>&</b>' is
being used.

 */
