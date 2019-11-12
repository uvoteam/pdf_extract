#include <zlib.h>

#include <string>
#include <map>

#include "common.h"


using namespace std;

enum {BLOCK_SIZE = 4096};

namespace
{
    string decompress_block(z_stream *strm, const string &data)
    {
        Bytef buffer[BLOCK_SIZE];
        char *src = const_cast<char*>(data.data());
        size_t src_len = data.length();
        string result;

        strm->avail_in = src_len;
        strm->next_in = reinterpret_cast<Bytef*>(src);
        do
        {
            strm->avail_out = BLOCK_SIZE;
            strm->next_out  = buffer;
            int ret = inflate(strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR ||
                ret == Z_NEED_DICT ||
                ret == Z_MEM_ERROR ||
                ret == Z_DATA_ERROR ||
                ret == Z_BUF_ERROR) throw pdf_error(FUNC_STRING + "inflate error");

            if (strm->avail_out < 0) throw pdf_error(FUNC_STRING + "decompressing error: avail_out <=0");
            result.append(reinterpret_cast<const char*>(buffer), BLOCK_SIZE - strm->avail_out);
        }
        while (strm->avail_out == 0);

        return result;
    }
}

string flate_decode(const string &data, const map<string, pair<string, pdf_object_t>> &opts)
{
    z_stream strm  = {0};
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;

    if (inflateInit(&strm) != Z_OK) throw pdf_error(FUNC_STRING + "inflateInit2 is not Z_OK");
    string result = decompress_block(&strm, data);
    inflateEnd(&strm);
    if (opts.empty()) return result;
    return predictor_decode(result, opts);
}
