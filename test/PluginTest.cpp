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

#define nm_help "help"
#define nm_config "reactor-config"
#define nm_input "input-dir"
#define nm_expected "expected-output-dir"
#define nm_failures "failures-dir"
#define nm_threads "num-threads"
#define nm_ignore "ignore"


int main(int argc, char* argv[])
{
    Fmi::Exception::force_stack_trace = true;

  try
  {
    namespace po = boost::program_options;

    std::vector<std::string> ignore_lists;

    po::options_description desc("Allowed options");

    desc.add_options()
        (nm_help ",h", "Show this help message")
        (nm_config ",c", po::value<std::string>(), "Reactor config file")
        (nm_input ",i", po::value<std::string>(), "Tests input directory (default 'input')")
        (nm_expected ",e", po::value<std::string>(), "Expected output directory (default 'output')")
        (nm_failures ",f", po::value<std::string>(), "Directory where to write actual output in"
            " case of failures (default value 'failures')")
        (nm_threads ",n", po::value<int>(), "Number of threads (default 10)")
        (nm_ignore ",I", po::value<std::vector<std::string> >(),
            "Optional parameter to specify files containing lists"
            " of tests to be skipped. File .testignore from input directory is used of none specified."
            " May be provided 0 or more times")
        ;

    po::variables_map opt;
    po::store(po::parse_command_line(argc, argv, desc), opt);

    po::notify(opt);

    if (opt.count(nm_help)) {
        std::cout << desc << std::endl;
        exit(1);
    }

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
        if (opt.count(nm_ignore)) {
            ignore_lists = opt[nm_ignore].as<std::vector<std::string> >();
        }

        for (const auto& fn : ignore_lists) {
            tester.addIgnoreList(fn);
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
