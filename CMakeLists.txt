cmake_minimum_required(VERSION 3.15)
project(passwordguard CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies (assuming they are installed via vcpkg)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(hello-imgui CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZXing REQUIRED)

# Gather source files
set(SOURCES
    src/core/2fa.cpp src/core/2fa.hpp
    src/core/crypto.cpp src/core/crypto.hpp
    src/gui.cpp
    src/core/password_input.cpp src/core/password_input.hpp
    src/main.cpp
    src/core/security.cpp src/core/security.hpp
    src/core/storage.cpp src/core/storage.hpp
    src/app/app_state.cpp src/app/app_state.hpp
    src/ui/theme/theme.cpp src/ui/theme/theme.hpp
    src/ui/layout/sidebar.cpp src/ui/layout/sidebar.hpp
    src/ui/pages/login_page.cpp src/ui/pages/login_page.hpp
    src/ui/pages/dashboard_page.cpp src/ui/pages/dashboard_page.hpp
    src/ui/pages/add_entry_page.cpp src/ui/pages/add_entry_page.hpp
    src/ui/pages/totp_page.cpp src/ui/pages/totp_page.hpp
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE src src/core)

# Link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    unofficial-sodium::sodium
    hello-imgui::hello_imgui
    imgui::imgui
    OpenSSL::Crypto
    ZXing::ZXing
    ${OpenCV_LIBS}
)

if(WIN32)
    # Required for DirectX 11 backend support implicitly required by the setup
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d11)
endif()
