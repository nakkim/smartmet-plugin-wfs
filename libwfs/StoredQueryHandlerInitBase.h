#pragma once

#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <boost/noncopyable.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WFS
{

class StoredQueryHandlerFactoryDef;

/**
 *   @brief Virtual base class for registering stored query handler initialization actions and executing them.
 */
class StoredQueryHandlerInitBase : public boost::noncopyable
{
protected:
    StoredQueryHandlerInitBase();

public:
    virtual ~StoredQueryHandlerInitBase();

    void execute_init_actions();

    void add_init_action(const std::string& name, std::function<void()> action);

private:
    std::queue<std::pair<std::string, std::function<void()> > > action_queue;
};

}  // namespace WFS
}  // namespace Plugin
}  // namespace SmartMet
