#include "WfsCapabilities.h"
#include <boost/foreach.hpp>
#include <macgyver/TypeName.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace bw = SmartMet::Plugin::WFS;
using namespace SmartMet::Spine;

using SmartMet::Spine::log_time_str;

bw::WfsCapabilities::WfsCapabilities() {}

bw::WfsCapabilities::~WfsCapabilities() {}

bool bw::WfsCapabilities::register_operation(const std::string& operation)
{
  try
  {
    SmartMet::Spine::WriteLock lock(mutex);
    return operations.insert(operation).second;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::set<std::string> bw::WfsCapabilities::get_operations() const
{
  try
  {
    SmartMet::Spine::WriteLock lock(mutex);
    return operations;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

const bw::WfsFeatureDef* bw::WfsCapabilities::find_feature(const std::string& name) const
{
  try
  {
    SmartMet::Spine::ReadLock lock(mutex);
    auto it = features.find(name);
    return it == features.end() ? nullptr : it->second.get();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::WfsCapabilities::register_feature(boost::shared_ptr<WfsFeatureDef>& feature_def)
{
  try
  {
    const auto name = feature_def->get_name();
    SmartMet::Spine::WriteLock lock(mutex);
    if (features.insert(std::make_pair(name, feature_def)).second)
    {
      std::ostringstream msg;
      msg << SmartMet::Spine::log_time_str() << ": [WFS] Feature '" << name << "' registered\n";
      std::cout << msg.str() << std::flush;
    }
    else
    {
      throw SmartMet::Spine::Exception(BCP, "The feature '" + name + "' is already registered!");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::map<std::string, boost::shared_ptr<bw::WfsFeatureDef> > bw::WfsCapabilities::get_features()
    const
{
  try
  {
    SmartMet::Spine::ReadLock lock(mutex);
    return features;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::WfsCapabilities::register_feature_use(const std::string& name)
{
  try
  {
    if (features.count(name))
    {
      // We do not care about repeated attempts here
      SmartMet::Spine::WriteLock lock(mutex);
      used_features.insert(name);
    }
    else
    {
      std::string sep = "";
      std::ostringstream msg;
      msg << "Available features are:";
      BOOST_FOREACH (const auto& item, features)
      {
        msg << sep << " '" << item.first << "'";
        sep = ",";
      }
      if (sep == "")
        msg << " <none>";

      SmartMet::Spine::Exception exception(BCP, "Feature '" + name + "' not found!");
      exception.addDetail(msg.str());
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::set<std::string> bw::WfsCapabilities::get_used_features() const
{
  try
  {
    SmartMet::Spine::ReadLock lock(mutex);
    return used_features;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void bw::WfsCapabilities::register_data_set(const std::string& code, const std::string& ns)
{
  try
  {
    SmartMet::Spine::WriteLock lock(mutex);
    if (not data_set_map.insert(std::make_pair(code, ns)).second)
    {
      std::ostringstream msg;
      msg << "Duplicate data set registration: code='" << code << "' namespace='" << ns << "'";
      throw SmartMet::Spine::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::map<std::string, std::string> bw::WfsCapabilities::get_data_set_map() const
{
  try
  {
    SmartMet::Spine::ReadLock lock(mutex);
    return data_set_map;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
