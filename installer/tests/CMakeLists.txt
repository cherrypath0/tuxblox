add_executable(test_install_paths test_install_paths.cpp)
target_link_libraries(test_install_paths PRIVATE installer_core)
add_test(NAME install_paths COMMAND test_install_paths)

add_executable(test_progress test_progress.cpp)
target_link_libraries(test_progress PRIVATE installer_core)
add_test(NAME progress COMMAND test_progress)

add_executable(test_checksum test_checksum.cpp)
target_link_libraries(test_checksum PRIVATE installer_core)
add_test(NAME checksum COMMAND test_checksum)

add_executable(test_manifest test_manifest.cpp)
target_link_libraries(test_manifest PRIVATE installer_core)
add_test(NAME manifest COMMAND test_manifest)

add_executable(test_downloader test_downloader.cpp)
target_link_libraries(test_downloader PRIVATE installer_core)
add_test(NAME downloader COMMAND test_downloader)

add_executable(test_tar_extract test_tar_extract.cpp)
target_link_libraries(test_tar_extract PRIVATE installer_core)
add_test(NAME tar_extract COMMAND test_tar_extract)

add_executable(test_installer_steps test_installer_steps.cpp)
target_link_libraries(test_installer_steps PRIVATE installer_core)
add_test(NAME installer_steps COMMAND test_installer_steps)
