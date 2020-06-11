#pragma once

#include "StoredQueryHandlerInitBase.h"
#include <spine/Reactor.h>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

class RequiresContourEngine : protected virtual StoredQueryHandlerInitBase
{
public:
  RequiresContourEngine(SmartMet::Spine::Reactor* reactor)
    {
        add_init_action(
            "Acquire Contour engine",
            [this, reactor]() {
                contour_engine = reactor->getEngine<SmartMet::Engine::Contour::Engine>("Contour");
            });
    }

protected:
  SmartMet::Engine::Contour::Engine* contour_engine;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
