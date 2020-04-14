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
    cmap_t() : is_vertical(false)
    {
    }
    std::unordered_map<std::string, std::string> utf16_map;
//how many bytes to read(see codespacerange)
    std::vector<unsigned char> sizes;
    bool is_vertical;
};

extern cmap_t get_cmap(const std::string &doc,
                       const ObjectStorage &storage,
                       const std::pair<unsigned int, unsigned int> &cmap_id_gen,
                       const dict_t &decrypt_data);

#endif //CMAP_H
