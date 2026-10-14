set(packagePluginDirectory "SKSE/Plugins")
set(packageLicenseDirectory "${packagePluginDirectory}/${PROJECT_NAME}/Licenses")

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${packagePluginDirectory}" COMPONENT ReleasePackage
    LIBRARY DESTINATION "${packagePluginDirectory}" COMPONENT ReleasePackage
)

install(FILES "${aftConfigurationFile}"
    DESTINATION "${packagePluginDirectory}"
    RENAME "ArbitraryFastTravel.ini" COMPONENT ReleasePackage
)

function(install_license sourcePath outputName)
    # Local testing archives contain only the plugin and testing INI.
    # Public packages still validate and include every required notice.
    if(AFT_USE_TESTING_INI)
        return()
    endif()

    if(NOT EXISTS "${sourcePath}")
        message(FATAL_ERROR "Required license file was not found: ${sourcePath}")
    endif()
    install(FILES "${sourcePath}"
        DESTINATION "${packageLicenseDirectory}"
        RENAME "${outputName}.txt"
        COMPONENT ReleasePackage
    )
endfunction()

install_license("${commonlibsse_SOURCE_DIR}/COPYING" "CommonLibSSE-NG-GPL-3.0")
install_license("${commonlibsse_SOURCE_DIR}/EXCEPTIONS.md" "CommonLibSSE-NG-Exceptions")

set(vcpkgShareDirectory "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")
install_license("${CMAKE_SOURCE_DIR}/LICENSE" "ArbitraryFastTravel")
install_license("${CMAKE_SOURCE_DIR}/LICENSING.md" "ArbitraryFastTravel-Licensing")
install_license("${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "ArbitraryFastTravel-ThirdPartyNotices")
install_license("${CMAKE_SOURCE_DIR}/SOURCES.md" "ArbitraryFastTravel-Sources")

foreach(dependency simpleini minhook directxmath directxtk fmt nlohmann-json rapidcsv spdlog toml11 xbyak)
    install_license("${vcpkgShareDirectory}/${dependency}/copyright" "${dependency}")
endforeach()

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
if(AFT_USE_TESTING_INI)
    string(APPEND CPACK_PACKAGE_FILE_NAME "-testing")
endif()
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};ReleasePackage;/")
include(CPack)
