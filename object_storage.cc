#include <string>
#include <map>
#include <utility>
#include <vector>

#include "object_storage.h"
#include "common.h"


using namespace std;

extern string decrypt(unsigned int n,
                      unsigned int g,
                      const string &in,
                      const dict_t &decrypt_opts);

ObjectStorage::ObjectStorage(const string &doc_arg, map<size_t, size_t> &&id2offsets_arg, const dict_t &decrypt_data) :
                             doc(doc_arg), id2offsets(move(id2offsets_arg))
{
    for (const pair<const size_t, size_t> &p : id2offsets) insert_obj_stream(p.first, decrypt_data);
}

pair<string, pdf_object_t> ObjectStorage::get_object(size_t id) const
{
    auto it = id2offsets.find(id);
    //object is located inside object stream
    if (it == id2offsets.end()) return id2obj_stm.at(id);
    return ::get_object(doc, id, id2offsets);
}

const map<size_t, size_t>& ObjectStorage::get_id2offsets() const
{
    return id2offsets;
}

size_t ObjectStorage::get_gen_id(size_t offset) const
{
    offset = efind_first(doc, " \r\t\n", offset);
    offset = efind_number(doc, offset);
    size_t end_offset = efind_first(doc, " \r\t\n", offset);
    return strict_stoul(doc.substr(offset, end_offset - offset));
}

void ObjectStorage::insert_obj_stream(size_t id, const dict_t &decrypt_data)
{
    size_t offset = id2offsets.at(id);
    offset = skip_comments(doc, offset);
    size_t gen_id = get_gen_id(offset);
    offset = skip_comments(doc, offset);
    offset = efind(doc, "obj", offset);
    offset += LEN("obj");
    if (get_object_type(doc, offset) != DICTIONARY) return;
    dict_t dictionary = get_dictionary_data(get_dictionary(doc, offset), 0);
    auto it = dictionary.find("/Type");
    if (it == dictionary.end() || it->second.first != "/ObjStm") return;
    unsigned int len = get_length<map<size_t, size_t>>(doc, id2offsets, dictionary);
    string content = get_content(doc, len, offset);
    content = decrypt(id, gen_id, content, decrypt_data);
    content = decode(content, dictionary);

    vector<pair<size_t, size_t>> id2offsets_obj_stm = get_id2offsets_obj_stm(content, dictionary);
    offset = strict_stoul(dictionary.at("/First").first);
    for (const pair<size_t, size_t> &p : id2offsets_obj_stm)
    {
        size_t obj_offset = offset + p.second;
        pdf_object_t type = get_object_type(content, obj_offset);
        id2obj_stm.emplace(p.first, make_pair(TYPE2FUNC.at(type)(content, obj_offset), type));
    }
}

vector<pair<size_t, size_t>> ObjectStorage::get_id2offsets_obj_stm(const string &content, const dict_t &dictionary)
{
    vector<pair<size_t, size_t>> result;
    size_t offset = 0;
    unsigned int n = strict_stoul(dictionary.at("/N").first);
    for (unsigned int i = 0; i < n; ++i)
    {
        offset = efind_number(content, offset);
        size_t end_offset = efind_first_not(content, "0123456789", offset);
        unsigned int id = strict_stoul(content.substr(offset, end_offset - offset));
        offset = efind_number(content, end_offset);
        end_offset = efind_first_not(content, "0123456789", offset);
        unsigned int obj_off = strict_stoul(content.substr(offset, end_offset - offset));
        result.emplace_back(id, obj_off);
        offset = end_offset;
    }

    return result;
}
