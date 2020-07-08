#ifndef CMAP_H
#define CMAP_H

#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

#include "object_storage.h"
#include "common.h"


struct cmap_t
{
    enum converted_status_t { CONVERTED, NOT_CONVERTED};
    enum {MAX_CODE_LENGTH = 4 /* 9.7.6.2 */ };

    cmap_t() : sizes(MAX_CODE_LENGTH + 1, 0)
    {
    }
    std::unordered_map<std::string, std::pair<converted_status_t, std::string>> utf_map;
    std::vector<unsigned char> sizes;
    bool is_vertical;
};

extern cmap_t get_cmap(const std::string &doc,
                       const ObjectStorage &storage,
                       const std::pair<unsigned int, unsigned int> &cmap_id_gen,
                       const dict_t &decrypt_data);

#endif //CMAP_H
