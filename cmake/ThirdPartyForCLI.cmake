set(CMDLINE_HPP_URL https://github.com/TianZerL/cmdline/raw/master/cmdline.hpp)
set(SHA1_CMDLINE "a3db560d700df5e4a19a40d43178f0839b9eec10")

set(INI17_HPP_URL https://github.com/TianZerL/ini17/raw/main/src/ini17.hpp)
set(SHA1_INI17 "d8ace387c8f010dee0155fdc93a1583e7672a2c8")

if(EXISTS ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp)
    file(SHA1 ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp LOCAL_SHA1_CMDLINE)

    if(NOT ${LOCAL_SHA1_CMDLINE} STREQUAL ${SHA1_CMDLINE})
        message("Warning:")
        message("   Local SHA1 for comline.hpp:   ${LOCAL_SHA1_CMDLINE}")
        message("   Expected SHA1:              ${SHA1_CMDLINE}")
        message("   Mismatch SHA1 for cmdline.hpp, trying to download it...")

        file(
            DOWNLOAD ${CMDLINE_HPP_URL} ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp 
            SHOW_PROGRESS 
            EXPECTED_HASH SHA1=${SHA1_CMDLINE}
        )

    endif()
else()
    file(
        DOWNLOAD ${CMDLINE_HPP_URL} ${TOP_DIR}/ThirdParty/include/cmdline/cmdline.hpp 
        SHOW_PROGRESS 
        EXPECTED_HASH SHA1=${SHA1_CMDLINE}
    )
endif()

if(EXISTS ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp)
    file(SHA1 ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp LOCAL_SHA1_INI17)

    if(NOT ${LOCAL_SHA1_INI17} STREQUAL ${SHA1_INI17})
        message("Warning:")
        message("   Local SHA1 for ini17.hpp:   ${LOCAL_SHA1_INI17}")
        message("   Expected SHA1:              ${SHA1_INI17}")
        message("   Mismatch SHA1 for ini17.hpp, trying to download it...")

        file(
            DOWNLOAD ${INI17_HPP_URL} ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp
            SHOW_PROGRESS 
            EXPECTED_HASH SHA1=${SHA1_INI17}
        )

    endif()
else()
    file(
        DOWNLOAD ${INI17_HPP_URL} ${TOP_DIR}/ThirdParty/include/ini17/ini17.hpp
        SHOW_PROGRESS 
        EXPECTED_HASH SHA1=${SHA1_INI17}
    )
endif()

find_package(CURL)
if(${CURL_FOUND})
    message(STATUS "CLI: libcurl found, enable web image download support.")
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
    add_definitions(-DENABLE_LIBCURL)
endif()

target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE
        ${TOP_DIR}/ThirdParty/include/cmdline
        ${TOP_DIR}/ThirdParty/include/ini17
)

target_link_libraries(${PROJECT_NAME} PRIVATE Anime4KCPPCore)

if(Use_Boost_filesystem)
    find_package(Boost COMPONENTS filesystem REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Boost::filesystem)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0) # Just for G++-8 to enable filesystem
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()
