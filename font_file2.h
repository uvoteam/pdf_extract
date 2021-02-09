#ifndef FONT_FILE2_H
#define FONT_FILE2_H

#include <string>
#include <utility>

#include "object_storage.h"
#include "common.h"


cmap_t get_FontFile2(const std::string &doc,
                     const ObjectStorage &storage,
                     const std::pair<unsigned int, unsigned int> &cmap_id_gen,
                     const dict_t &decrypt_data);


#endif //FONT_FILE2_H
