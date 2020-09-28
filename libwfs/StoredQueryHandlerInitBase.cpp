#include "StoredQueryHandlerInitBase.h"
#include <set>
#include <macgyver/Exception.h>

using Fmi::Exception;
using SmartMet::Plugin::WFS::StoredQueryHandlerInitBase;

StoredQueryHandlerInitBase::StoredQueryHandlerInitBase()
    : action_queue()
{
}

StoredQueryHandlerInitBase::~StoredQueryHandlerInitBase()
{
}

void StoredQueryHandlerInitBase::execute_init_actions()
{
    std::set<std::string> processed_names;
    while (not action_queue.empty()) {
        const auto item = action_queue.front();
        action_queue.pop();
        try {
            if (processed_names.insert(item.first).second) {
                item.second();
            } else {
                throw Exception::Trace(BCP,
                    "INTERNAL ERROR: Init operation '"
                    + item.first + "' queued more than once");
            }
        } catch (...) {
            throw Exception::Trace(BCP, "Operation failed")
                .addDetail(item.first);
        }
    }
}

void StoredQueryHandlerInitBase::add_init_action(
    const std::string& name,
    std::function<void()> action)
{
    action_queue.push(std::make_pair(name, action));
}

