#pragma once

#include "PluginImpl.h"
#include "StoredQueryConfig.h"
#include "StoredQueryHandlerBase.h"
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{
class PluginImpl;

class StoredQueryHandlerFactoryDef
{
 public:
  typedef boost::shared_ptr<StoredQueryHandlerBase> (*factory_t)(
      SmartMet::Spine::Reactor* reactor,
      StoredQueryConfig::Ptr config,
      PluginImpl& plugin_impl,
      boost::optional<std::string> template_file_name);

 private:
  unsigned char signature[20];
  factory_t factory;

 public:
  StoredQueryHandlerFactoryDef(factory_t factory);

  virtual ~StoredQueryHandlerFactoryDef();

  static boost::shared_ptr<StoredQueryHandlerBase> construct(
      const std::string& symbol_name,
      SmartMet::Spine::Reactor* reactor,
      StoredQueryConfig::Ptr config,
      PluginImpl& plugin_impl,
      boost::optional<std::string> template_file_name);

 private:
  static void create_signature(unsigned char* md);
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
