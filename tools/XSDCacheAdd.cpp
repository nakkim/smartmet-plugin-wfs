#include <cassert>
#include <iostream>
#include <sstream>
#include <boost/filesystem.hpp>
#include <openssl/sha.h>

namespace fs = boost::filesystem;

int main(int argc, char* argv[])
{
    assert(argc == 3);
    const std::string fn = argv[1];
    const std::string uri = argv[2];

    std::ostringstream fn_str;
    unsigned char md[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(uri.c_str()), uri.length(), md);
    fn_str << "local_cache/";
    for (unsigned i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        char tmp[3];
        snprintf(tmp, 3, "%02X", md[i]);
        fn_str << tmp;
    }
    const std::string output = fn_str.str();

    fs::create_directories("local_cache");
    fs::copy_file(fn, output);
    return 0;
}
