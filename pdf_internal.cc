#include <exception>
#include <string>
#include <vector>
#include <map>

#include "pdf_internal.h"
#include "pdf_extractor.h"

using namespace std;

namespace
{
    unsigned int get_decode_key(const map<string, pair<string, pdf_object_t>> &opts, const string &key, unsigned int def)
    {
        auto it = opts.find(key);
        if (it == opts.end()) return def;

        return strict_stoul(it->second.first);
    }
}

string predictor_decode(const string &data, const map<string, pair<string, pdf_object_t>> &opts)
{
    unsigned int predictor = get_decode_key(opts, "/Predictor", 1);
    unsigned int colors = get_decode_key(opts, "/Colors", 1);
    unsigned int BPCs = get_decode_key(opts, "/BitsPerComponent", 8);
    unsigned int columns     = get_decode_key(opts, "/Columns", 1);
    unsigned int early_change = get_decode_key(opts, "/EarlyChange", 1);
    bool next_byte_is_predictor = predictor >= 10? true: false;
    unsigned int cur_predictor = predictor >= 10? -1 : predictor;
    unsigned int cur_row_index  = 0;
    unsigned int bpp  = (BPCs * colors) >> 3;
    unsigned int rows = (columns * colors * BPCs) >> 3;
    vector<char> prev(rows, 0);

    if (predictor == 1) return data;

    const char *p_buffer = data.c_str();
    size_t len = data.length();
    string result;
    while (len--)
    {
        if (next_byte_is_predictor)
        {
            cur_predictor = *p_buffer + 10;
            next_byte_is_predictor = false;
        }
        else
        {
            switch (cur_predictor )
            {
            case 2: // Tiff Predictor
            {
                if (BPCs == 8)
                {   // Same as png sub
                    int prev_local = cur_row_index - bpp < 0 ? 0 : prev[cur_row_index - bpp];
                    prev[cur_row_index] = *p_buffer + prev_local;
                    break;
                }

                throw pdf_error(FUNC_STRING + "tiff predictor other than 8 BPC is not implemented");
                break;
            }
            case 10: // png none
            {
                prev[cur_row_index] = *p_buffer;
                break;
            }
            case 11: // png sub
            {
                int local_prev = cur_row_index - bpp < 0? 0 : prev[cur_row_index - bpp];
                prev[cur_row_index] = *p_buffer + local_prev;
                break;
            }
            case 12: // png up
            {
                prev[cur_row_index] += *p_buffer;
                break;
            }
            case 13: // png average
            {
                int local_prev = cur_row_index - bpp < 0? 0 : prev[cur_row_index - bpp];
                prev[cur_row_index] = ((local_prev + prev[cur_row_index]) >> 1) + *p_buffer;
                break;
            }
            case 14: // png paeth
            case 15: // png optimum
            {
                throw pdf_error(FUNC_STRING + "predictor " + to_string(cur_predictor) + " is invalid");
                break;
            }
            default:
            {
                //    throw pdf_error(FUNC_STRING + "predictor " + to_string(cur_predictor) + " is invalid");
                break;
            }
            }
            ++cur_row_index;
        }
        ++p_buffer;

        if (cur_row_index >= rows )
        {   // One line finished
            cur_row_index  = 0;
            next_byte_is_predictor = (cur_predictor >= 10);
            result.append(prev.data(), rows);
        }
    }

    return result;
}

size_t strict_stoul(const string &str)
{
    if (str.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    size_t pos;
    if (str.find('-') != string::npos) throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    size_t val;
    try
    {
        val = stoul(str, &pos);
    }
    catch (const std::exception &e)
    {
        throw pdf_error(FUNC_STRING + str + " is not unsigned number");
    }

    if (pos != str.size()) throw pdf_error(FUNC_STRING + str + " is not unsigned number");

    return val;
}

long int strict_stol(const string &str)
{
    if (str.empty()) throw pdf_error(FUNC_STRING + "string is empty");
    size_t pos;
    size_t val;
    try
    {
        val = stol(str, &pos);
    }
    catch (const std::exception &e)
    {
        throw pdf_error(FUNC_STRING + str + " is not number");
    }

    if (pos != str.size()) throw pdf_error(FUNC_STRING + str + " is not number");

    return val;
}
