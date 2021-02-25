#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "cmap.h"

using namespace std;

string get_name2unicode(const string &name)
{
    static const unordered_map<string, string> name2unicode = {
          #include "symbol_table.h"
    };
    auto it = name2unicode.find(name);
    if (it != name2unicode.end()) return it->second;
    return "";
}

void get_binary(string &source)
{
    for (char &c : source) c -= '0';
}

cmap_t get_FontFile(const string &doc,
                    const ObjectStorage &storage,
                    const pair<unsigned int, unsigned int> &cmap_id_gen,
                    const dict_t &decrypt_data)
{
        const string stream = get_stream(doc, cmap_id_gen, storage, decrypt_data);
        cmap_t cmap;
        cmap.is_vertical = false;
        vector<string> st;
        for (size_t i = skip_comments(stream, 0, false);
             i != string::npos && i < stream.length();
             i = skip_comments(stream, i, false))
        {
            string token = get_token(stream, i);
            if (st.empty())
            {
                st.push_back(std::move(token));
                continue;
            }
            if (token == "eexec" && st.back() == "currentfile") return cmap;
            if (token == "put")
            {
                const string result = get_name2unicode(pop(st));
                string source = pop(st);
                get_binary(source);
                cmap.utf_map.emplace(source, make_pair(cmap_t::CONVERTED, result));
                continue;
            }
            st.push_back(std::move(token));
        }
        return cmap;
}
