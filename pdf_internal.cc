#include <exception>

#include "pdf_internal.h"
#include "pdf_extractor.h"

using namespace std;

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
