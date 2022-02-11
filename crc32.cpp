#include "crc32.h"
#include <fstream>
#include <boost/crc.hpp>

#define BUFFER_SIZE 1024

uint32_t calc_file_crc32(const char *dir, const char *file) {
    /// TODO check args
    try {
        std::string full_name = std::string(dir) + "/" + std::string(file);
        std::ifstream ifs(full_name.c_str(), std::ios_base::binary);
        if (ifs) {
            boost::crc_32_type result;
            do {
                char buffer[BUFFER_SIZE];
                ifs.read(buffer, BUFFER_SIZE);
                result.process_bytes(buffer, static_cast<std::size_t>(ifs.gcount()));
            } while (ifs);
            return result.checksum();
        }
        // bad file
        return 0;
    } catch (...) {}
    // return when exception
    return 0;
}
