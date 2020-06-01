#pragma once

#include "StoredQueryHandlerInitBase.h"
#include <spine/Reactor.h>
#include <engines/observation/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

class RequiresObsEngine : protected virtual StoredQueryHandlerInitBase
{
public:
  RequiresObsEngine(SmartMet::Spine::Reactor* reactor)
    {
        add_init_action(
            "Acquire ObsEngine",
            [this, reactor] () {
                obs_engine = reactor->getEngine<SmartMet::Engine::Observation::Engine>("Observation");
            });
    }

protected:
  SmartMet::Engine::Observation::Engine* obs_engine;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
