#pragma once

#include <boost/shared_ptr.hpp>

namespace SmartMet
{
  namespace Plugin
  {
    namespace WFS
    {

      class StoredQueryConfig;

      typedef boost::shared_ptr<StoredQueryConfig> StoredQueryConfigPtr;

      class StoredQueryConfigWrapper
      {
      public:
        StoredQueryConfigWrapper(StoredQueryConfigPtr config_p) : config_p(config_p) {}
        virtual ~StoredQueryConfigWrapper() {}
        StoredQueryConfigPtr get_config() const { return config_p; }
      private:
        StoredQueryConfigPtr config_p;
      };

    }  // namespace WFS
  }  // namespace Plugin
}  // namespace SmartMet
