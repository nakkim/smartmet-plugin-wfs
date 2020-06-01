#pragma once

#include "StoredQueryHandlerInitBase.h"
#include <spine/Reactor.h>
#include <engines/grid/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

class RequiresGridEngine : protected virtual StoredQueryHandlerInitBase
{
public:
  RequiresGridEngine(SmartMet::Spine::Reactor* reactor)
    {
        add_init_action(
            "Acquire Grid engine",
            [this, reactor]() {
                grid_engine = reactor->getEngine<SmartMet::Engine::Grid::Engine>("grid");
            });
    }

protected:
  SmartMet::Engine::Grid::Engine* grid_engine;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
