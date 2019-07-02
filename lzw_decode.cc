#include <string>
#include <vector>
#include <cstdlib>


using namespace std;

struct DecompressData
{
    wstring str;
    int val, pos, indx;
    DecompressData(): str(L""), val(0), pos(0), indx(0)
    {
    }
    DecompressData(wstring &s_, int v_, int p_, int i_): str(s_), val(v_), pos(p_), indx(i_)
    {
    }
};

int read_bit(DecompressData &data)
{
    int rst = data.val & data.pos;
    if ((data.pos >>= 1) == 0)
    {
        data.pos = 32768;
        if (data.indx < static_cast<int>(data.str.length())) data.val = data.str[data.indx++];
    }
    return rst > 0 ? 1:0;
}

long ipow(double x, int n)
{
    if (!n) return 1;
    if (n < 0)
    {
        n = -n;
        x = 1.0/x;
    }
    long r = ipow(x, n/2);

    return (long)(n & 1 ? r*r*x : r*r);
}

int read_bits(int numbits, DecompressData &data)
{
    int rst = 0, maxp = ipow(2, numbits), p = 1;
    while (p != maxp) {
        rst |= read_bit(data)*p; p <<= 1;
    }
    return rst;
}

wstring tostr(int val)
{
    wstring str;
    str = wchar_t(val);

    return str;
}

wstring Decompress(wstring &compressed)
{
    if (compressed.empty()) return L"";

    DecompressData data(compressed, compressed[0], 32768, 1);
    vector<wstring> dictionary; dictionary.reserve(compressed.size());
    wstring entry, w, sc, rst;
    int next = 0, enlarge_in = 4, numbits = 3, c = 0, err_cnt = 0;
    for (int i = 0; i < 3; ++i) dictionary.push_back(tostr(i));
    next = read_bits(2, data);
    switch(next) {
    case 0: sc = wchar_t(read_bits(8, data)); break;
    case 1: sc = wchar_t(read_bits(16, data)); break;
    case 2: return L"";
    }
    dictionary.push_back(sc);
    w = sc, rst += sc;

    while (true) {
        c = read_bits(numbits, data);
        int cc = c;
        switch(cc) {
        case 0:
        case 1:
            if (!cc && ++err_cnt > 10000) return L""; /*! too many errors */
            sc = wchar_t(read_bits(1<<(cc+3), data));
            dictionary.push_back(sc);
            c = dictionary.size()-1; --enlarge_in;
            break;
        case 2: return rst;
        }

        if (!enlarge_in) enlarge_in = ipow(2, numbits++);
        if ((int)dictionary.size() > c && !dictionary[c].empty())
        {
            entry = dictionary[c];
        }
        else
        {
            if (c == dictionary.size()) entry = w + wchar_t(w[0]);
            else return L"";
        }

        rst += entry;
        dictionary.push_back(w + wchar_t(entry[0]));
        w = entry;
        if (!--enlarge_in) enlarge_in = ipow(2, numbits++);
    }
}

string ToStr(const wstring& wstr)
{
    if (wstr.empty()) return "";
    try {
        //setlocale(LC_ALL,".ACP");
        const wchar_t *wptr = wstr.c_str();
        size_t msize = (wstr.size()+1)*sizeof(wchar_t)/sizeof(char);
        char *ptr = new char[msize];
        int asize = wcstombs(ptr, wstr.c_str(), msize);
        string str(ptr);
        delete[] ptr;
        return str;
        //string str(wstr.size(), 0);
        //copy(wstr.begin(), wstr.end(), str.begin());
        //return str;
    } catch(...) {
        return "";
    }
}

wstring ToWStr(const std::string& str)
{
    if (str.empty()) return L"";
    try {
        wchar_t *wptr = new wchar_t[str.size()+1];
        size_t asize = mbstowcs(wptr, str.c_str(), (str.size()+1)*sizeof(wchar_t));
        wstring wstr(wptr);
        delete [] wptr; wptr = nullptr;
        return wstr;
    }
    catch(...)
    {
        return L"";
    }
}

string lzw_decode(const string &compressed)
{
    wstring wstr = ToWStr(compressed);
    return ToStr(Decompress(wstr));
}
