#include "Plugin.h"
#include <spine/PluginTest.h>
#include <iostream>
#include <sstream>

using namespace std;

void prelude(SmartMet::Spine::Reactor& reactor)
{
  try
  {
    // Wait for qengine to finish
    auto handlers = reactor.getURIMap();
    while (handlers.find("/wfs") == handlers.end())
    {
      sleep(1);
      handlers = reactor.getURIMap();
    }

    cout << endl << "Testing WFS plugin" << endl << "============================" << endl;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Prelude failed!", NULL);
  }
}

int main(int argc, char* argv[])
{
  try
  {
    SmartMet::Spine::Options options;
    options.configfile = argc == 1 ? "cnf/wfs_plugin_test.conf" : argv[1];
    options.quiet = true;
    options.defaultlogging = false;
    options.accesslogdir = "/tmp";

    try
    {
      return SmartMet::Spine::PluginTest::test(options, prelude, false, 10);
    }
    catch (const libconfig::ParseException& err)
    {
      std::ostringstream msg;
      msg << "Exception '" << Fmi::current_exception_type()
          << "' thrown while parsing configuration file " << options.configfile << "' at line "
          << err.getLine() << ": " << err.getError();
      throw Fmi::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", NULL);
  }

  return 1;
}
