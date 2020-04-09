#pragma once

#include "StoredQueryHandlerInitBase.h"
#include <spine/Reactor.h>
#include <engines/querydata/Engine.h>
namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

class RequiresQEngine : protected virtual StoredQueryHandlerInitBase
{
public:
  RequiresQEngine(SmartMet::Spine::Reactor* reactor)
    {
        add_init_action(
            "Acquire QEngine",
            [this, reactor]() {
                q_engine = reactor->getEngine<SmartMet::Engine::Querydata::Engine>("Querydata");
            });
    }

protected:
  SmartMet::Engine::Querydata::Engine* q_engine;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
