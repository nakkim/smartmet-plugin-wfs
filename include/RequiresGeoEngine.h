#pragma once

#include "StoredQueryHandlerInitBase.h"
#include <spine/Reactor.h>
#include <engines/geonames/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

class RequiresGeoEngine : protected virtual StoredQueryHandlerInitBase
{
public:
  RequiresGeoEngine(SmartMet::Spine::Reactor* reactor)
    {
        add_init_action(
            "Acquire Geoengine",
            [this, reactor]() {
                geo_engine = reactor->getEngine<SmartMet::Engine::Geonames::Engine>("Geonames");
            });
    }

protected:
  SmartMet::Engine::Geonames::Engine* geo_engine;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
