cmake_minimum_required(VERSION 3.22)
project(pdf_extractor)
set(PROGRAM_NAME pdf_extractor)
set(LIB_VERSION_STRING 1.0.10)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -O2")
set(SOURCES ascii85_decode.cc
            ascii_hex_decode.cc
            charset_converter.cc
            cmap.cc
            common.cc
            converter_data.cc
            converter_engine.cc
            coordinates.cc
            decrypt.cc
            diff_converter.cc
            flate_decode.cc
            fonts.cc
            lzw_decode.cc
            object_storage.cc
            pages_extractor.cc
            font_file2.cc
            font_file.cc
            parser.cc
            to_unicode_converter.cc)

find_library(BOOST_SYSTEM boost_system REQUIRED)
find_library(BOOST_LOCALE boost_locale REQUIRED)
find_library(LIBZ z REQUIRED)
find_package(OpenSSL 3.0 REQUIRED)

add_library(${PROGRAM_NAME} SHARED ${SOURCES})
target_link_libraries(${PROGRAM_NAME}
                      ${BOOST_SYSTEM}
                      ${BOOST_LOCALE}
                      crypto
                      ${LIBZ})
install(TARGETS ${PROGRAM_NAME}
        LIBRARY DESTINATION lib COMPONENT libraries)
install(FILES pdf_extractor.h DESTINATION include)
