#include "Plugin.h"
#include <spine/PluginTest.h>
#include <iostream>
#include <sstream>
#include <boost/program_options.hpp>

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
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");

    const char* nm_config = "reactor-config";
    const char* nm_input = "input-dir";
    const char* nm_expected = "expected-output-dir";
    const char* nm_failures = "failures-dir";
    const char* nm_threads = "num-threads";

    desc.add_options()
        (nm_config, po::value<std::string>(), "Reactor config file")
        (nm_input, po::value<std::string>(), "Tests input directory (default 'input')")
        (nm_expected, po::value<std::string>(), "Expected output directory (default 'output')")
        (nm_failures, po::value<std::string>(), "Directory where to write actual output in"
            " case of failures (default value 'failures')")
        (nm_threads, po::value<int>(), "Number of threads (default 10)")
        ;

    po::variables_map opt;
    po::store(po::parse_command_line(argc, argv, desc), opt);

    po::notify(opt);

    SmartMet::Spine::Options options;
    options.configfile = opt.count(nm_config)
        ? opt[nm_config].as<std::string>()
        : std::string("cnf/wfs_plugin_test.conf");
    options.quiet = true;
    options.defaultlogging = false;
    options.accesslogdir = "/tmp";

    try
    {
        SmartMet::Spine::PluginTest tester;
        if (opt.count(nm_input)) {
            tester.setInputDir(opt[nm_input].as<std::string>());
        }
        if (opt.count(nm_expected)) {
            tester.setOutputDir(opt[nm_expected].as<std::string>());
        }
        if (opt.count(nm_failures)) {
            tester.setFailDir(opt[nm_failures].as<std::string>());
        }
        if (opt.count(nm_threads)) {
            tester.setNumberOfThreads(opt[nm_threads].as<int>());
        }

        return tester.run(options, prelude);
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
