/*
 * TypeNameStoredQueryMap.cpp
 *
 *  Created on: Nov 10, 2014
 *      Author: niemitu
 */
#include "TypeNameStoredQueryMap.h"
#include "WfsException.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <sstream>
#include <stdexcept>

namespace ba = boost::algorithm;
namespace bw = SmartMet::Plugin::WFS;

bw::TypeNameStoredQueryMap::TypeNameStoredQueryMap() {}

bw::TypeNameStoredQueryMap::~TypeNameStoredQueryMap() {}

void bw::TypeNameStoredQueryMap::init(const std::map<std::string, std::string>& typename_storedqry)
{
  try
  {
    BOOST_FOREACH (const auto& item, typename_storedqry)
    {
      const std::string& type_name = Fmi::ascii_tolower_copy(item.first);

      std::vector<std::string> qrys;
      ba::split(qrys, item.second, ba::is_any_of(";"));

      typename_qry_map[type_name] = qrys;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::vector<std::string>& bw::TypeNameStoredQueryMap::get_stored_queries_by_name(
    const std::string& name) const
{
  try
  {
    const std::string& lname = Fmi::ascii_tolower_copy(name);

    auto loc = typename_qry_map.find(lname);
    if (loc == typename_qry_map.end())
    {
      SmartMet::Spine::Exception exception(BCP, "Cannot find a stored query by its name!");
      exception.addParameter(WFS_EXCEPTION_CODE, WFS_OPERATION_PARSING_FAILED);
      exception.addParameter("Stored query name", name);
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
