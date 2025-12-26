cmake_minimum_required(VERSION 3.20)
project(InfoDash VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)

# Source files
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/ui/MainWindow.cpp
    src/ui/RSSPanel.cpp
    src/ui/WeatherPanel.cpp
    src/ui/StockPanel.cpp
    src/services/RSSService.cpp
    src/services/WeatherService.cpp
    src/services/StockService.cpp
    src/utils/HttpClient.cpp
    src/utils/HtmlParser.cpp
    src/utils/Config.cpp
    src/utils/ThemeManager.cpp
)

# Header files
set(HEADERS
    include/app/Application.hpp
    include/ui/MainWindow.hpp
    include/ui/RSSPanel.hpp
    include/ui/WeatherPanel.hpp
    include/ui/StockPanel.hpp
    include/services/RSSService.hpp
    include/services/WeatherService.hpp
    include/services/StockService.hpp
    include/utils/HttpClient.hpp
    include/utils/HtmlParser.hpp
    include/utils/Config.hpp
    include/utils/ThemeManager.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTK4_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    ${JSON_GLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${GTK4_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    pthread
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    ${GTK4_CFLAGS_OTHER}
    ${LIBCURL_CFLAGS_OTHER}
    ${LIBXML2_CFLAGS_OTHER}
    -Wall -Wextra -Wpedantic
)

# Install
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/ DESTINATION share/${PROJECT_NAME})

# Copy resources to build directory
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})

# Small test target for RSS autodiscovery
add_executable(rss_autodiscover_test tests/rss_autodiscover_test.cpp)
target_include_directories(rss_autodiscover_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTK4_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    ${JSON_GLIB_INCLUDE_DIRS}
)
target_link_libraries(rss_autodiscover_test PRIVATE
    ${GTK4_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    pthread
)
target_compile_options(rss_autodiscover_test PRIVATE
    ${GTK4_CFLAGS_OTHER}
    ${LIBCURL_CFLAGS_OTHER}
    ${LIBXML2_CFLAGS_OTHER}
    -Wall -Wextra -Wpedantic
)

add_executable(stock_debug_test tests/stock_debug_test.cpp)
target_include_directories(stock_debug_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTK4_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    ${JSON_GLIB_INCLUDE_DIRS}
)
target_link_libraries(stock_debug_test PRIVATE
    ${GTK4_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    pthread
)
target_compile_options(stock_debug_test PRIVATE
    ${GTK4_CFLAGS_OTHER}
    ${LIBCURL_CFLAGS_OTHER}
    ${LIBXML2_CFLAGS_OTHER}
    -Wall -Wextra -Wpedantic
)
target_sources(stock_debug_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/services/StockService.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/HttpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/HtmlParser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Config.cpp
)

# Build RSSService and helpers into the test target so it links standalone
target_sources(rss_autodiscover_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/services/RSSService.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/HttpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/HtmlParser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Config.cpp
)
