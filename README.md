pdf_extract is a thread-safe library to extract text from pdf files. Vertical text is not rendered. Multicolumn
document is rendered vertically(each column is drawed separated by \n).

You should call pdf_extractor_init to init library before usage, and pdf_extractor_deinit after usage;
std::string pdf2txt(const std::string &buffer);

Input argument:
buffer - content of pdf file

Output:
extracted utf8 text

In case of error std::exception is thrown

Example:

#include <fstream>
#include <iostream>

#include <pdf_extractor.h>

using namespace std;

int main(int argc, char *argv[])
{
    pdf_extractor_init(); //should be called only one time, not thread safe
    std::ifstream t(argv[1]);
    std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
    cout << pdf2txt(str); //thread safe
    pdf_extractor_deinit(); //should be called only one time, not thread safe
    return 0;
}







Build:(cmake, libssl 3, boost_system, boost_locale, libz are required)

1. mkdir build

2. cd ./build

3.cmake ..

4.make

5.make install


pdf_extract is distributed under GNU Public License version 2 or above.
