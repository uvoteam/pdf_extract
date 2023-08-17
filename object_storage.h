#ifndef OBJECT_STORAGE_H
#define OBJECT_STORAGE_H

#include <string>
#include <map>
#include <utility>
#include <vector>

#include "common.h"

class ObjectStorage
{
public:
    ObjectStorage(const std::string &doc_arg, std::map<size_t, size_t> &&id2offsets_arg, const dict_t &encrypt_data);
    std::pair<std::string, pdf_object_t> get_object(size_t id) const;
    const std::map<size_t, size_t>& get_id2offsets() const;
    bool is_object_exists(size_t id) const;
private:
    size_t get_gen_id(size_t offset) const;
    void insert_obj_stream(size_t id, const dict_t &encrypt_data);
    std::vector<std::pair<size_t, size_t>> get_id2offsets_obj_stm(const std::string &content, const dict_t &dictionary);
private:
    const std::string &doc;
    std::map<size_t, size_t> id2offsets;
    //7.5.7. Object Streams
    std::map<size_t, std::pair<std::string, pdf_object_t>> id2obj_stm;
};

#endif //OBJECT_STORAGE_H
