#include "RequestParameterMap.h"
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <spine/Exception.h>
#include <algorithm>

namespace bw = SmartMet::Plugin::WFS;

bw::RequestParameterMap::RequestParameterMap() : params() {}

bw::RequestParameterMap::RequestParameterMap(
    const std::multimap<std::string, SmartMet::Spine::Value>& params)
    : params(params)
{
}

bw::RequestParameterMap::~RequestParameterMap() {}

void bw::RequestParameterMap::clear()
{
  try
  {
    params.clear();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t bw::RequestParameterMap::size() const
{
  try
  {
    return params.size();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t bw::RequestParameterMap::count(const std::string& name) const
{
  try
  {
    return params.count(name);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestParameterMap::insert_value(const std::string& name,
                                           const SmartMet::Spine::Value& value)
{
  try
  {
    params.insert(std::make_pair(name, value));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::set<std::string> bw::RequestParameterMap::get_keys() const
{
  try
  {
    std::set<std::string> result;
    std::transform(params.begin(),
                   params.end(),
                   std::inserter(result, result.begin()),
                   boost::bind(&std::pair<const std::string, SmartMet::Spine::Value>::first, ::_1));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<SmartMet::Spine::Value> bw::RequestParameterMap::get_values(
    const std::string& name) const
{
  try
  {
    std::vector<SmartMet::Spine::Value> result;
    auto range = params.equal_range(name);
    std::transform(
        range.first,
        range.second,
        std::back_inserter(result),
        boost::bind(&std::pair<const std::string, SmartMet::Spine::Value>::second, ::_1));
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestParameterMap::dump_params(CTPP::CDT& hash) const
{
  try
  {
    const auto keys = get_keys();
    for (auto it1 = keys.begin(); it1 != keys.end(); ++it1)
    {
      int ind = 0;
      const std::string& key = *it1;
      auto range = params.equal_range(key);
      for (auto it2 = range.first; it2 != range.second; ++it2)
      {
        const auto str = it2->second.to_string();
        hash[key][ind++] = str;
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::RequestParameterMap::remove(const std::string& name)
{
  try
  {
    params.erase(name);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::RequestParameterMap::as_string() const
{
  try
  {
    const auto keys = get_keys();
    std::ostringstream output;
    output << "(PARAMETERS";
    BOOST_FOREACH (const auto& key, keys)
    {
      output << " (" << key;
      auto range = params.equal_range(key);
      for (auto map_it = range.first; map_it != range.second; ++map_it)
      {
        output << " " << map_it->second;
      }
      output << ')';
    }
    output << ')';
    return output.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string bw::RequestParameterMap::subst(const std::string& input) const
{
  try
  {
    std::ostringstream result;

    bool fail = false;
    const char* src = input.c_str();

    while (*src and not fail)
    {
      if (src[0] == '\\')
      {
        if (not src[1])
        {
          throw SmartMet::Spine::Exception(BCP, "Unexpected end of source string '" + input + "'!");
        }
        result << src[1];
        src += 2;
      }
      else if (src[0] == '$')
      {
        bool is_optional = false;
        bool is_array = false;

        if (src[1] != '{')
        {
          throw SmartMet::Spine::Exception(BCP, "Missing '{' after '$' in '" + input + "'!");
        }

        src += 2;

        if (*src == '?')
        {
          is_optional = true;
          src++;
        }
        else if (*src == '*')
        {
          is_array = true;
          src++;
        }

        const char* name = src;
        while (std::isalnum(*src) or *src == '_')
        {
          src++;
        }

        if (*src != '}')
        {
          throw SmartMet::Spine::Exception(
              BCP, "Invalid parameter name after '${' or closing '}' missing in '" + input + "'!");
        }

        const std::string ref_name(name, src);
        src++;

        std::vector<SmartMet::Spine::Value> data;
        auto range = params.equal_range(ref_name);
        std::transform(
            range.first,
            range.second,
            std::back_inserter(data),
            boost::bind(&std::pair<const std::string, SmartMet::Spine::Value>::second, ::_1));

        if (is_array)
        {
          std::string sep = "";
          for (auto it = data.begin(); it != data.end(); ++it)
          {
            result << sep << it->to_string();
            sep = ",";
          }
        }
        else
        {
          if (data.size() > 1)
          {
            std::ostringstream msg;
            msg << "Too many (" << data.size() << ") values of"
                << " '" << ref_name << "' in '" << input << "'";
            throw SmartMet::Spine::Exception(BCP, msg.str());
          }

          if (not is_optional and data.empty())
          {
            std::ostringstream msg;
            msg << ": mandatory value of '" << ref_name << "' missing"
                << " in '" << input << "'";
            throw SmartMet::Spine::Exception(BCP, msg.str());
          }

          if (not data.empty())
          {
            result << data.begin()->to_string();
          }
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
