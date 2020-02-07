#include <cassert>
#include <iostream>
#include "FeatureID.h"

using namespace SmartMet::Plugin::WFS;

int main(int argc, char** argv)
{
  assert(argc >= 1);

  for (int i = 1; i < argc; i++) {
    try {
      std::cout << "Decoding feature ID: " << argv[i] << std::endl;
      boost::shared_ptr<FeatureID> id = FeatureID::create_from_id(argv[i]);
      std::cout << "    storedquery_id: " << id->get_stored_query_id() << std::endl;
      for (const auto& item : id->get_params()) {
	std::string nm = item.first;
	std::cout << "    parameter: " << nm;
	if (nm.length() < 32) {
	  std::cout << std::string(32-nm.length(), ' ');
	}
	std::cout << " value:";
	item.second.print_on(std::cout);
	std::cout << std::endl;
      }
    } catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
    }
  }

  return 0;
}
