include("${VITASDK}/share/vita.cmake" REQUIRED)
include("${CMAKE_SOURCE_DIR}/cmake/vita-version.cmake")

# Deployment reads the generated title-id.txt rather than duplicating this ID.
set(LBA2_VITA_TITLE_ID "LBA2CC001")
string(STRIP "${_LBA2_VER_RAW}" _VITA_ENGINE_VERSION)
lba2_vita_package_version("${_VITA_ENGINE_VERSION}" LBA2_VITA_APP_VERSION)
file(WRITE "${CMAKE_BINARY_DIR}/packaging/vita-title-id.txt" "${LBA2_VITA_TITLE_ID}\n")
configure_file("${CMAKE_SOURCE_DIR}/packaging/vita-readme.txt.in"
    "${CMAKE_BINARY_DIR}/packaging/vita-README.txt" @ONLY)

vita_create_self("${LBA2_EXECUTABLE_NAME}.self" ${LBA2_EXECUTABLE_NAME} STRIPPED)
set(_VITA_DIAGNOSTIC_FILES)
if(LBA2_VITA_DIAGNOSTICS)
    list(APPEND _VITA_DIAGNOSTIC_FILES FILE
        "${CMAKE_SOURCE_DIR}/packaging/vita-debugnet-notice.txt" DEBUGNET-NOTICE.txt)
endif()
vita_create_vpk("${LBA2_EXECUTABLE_NAME}-vita.vpk" "${LBA2_VITA_TITLE_ID}"
    "${LBA2_EXECUTABLE_NAME}.self"
    VERSION "${LBA2_VITA_APP_VERSION}" NAME "${LBA2_PRODUCT_NAME}"
    FILE "${CMAKE_SOURCE_DIR}/packaging/vita/icon0.png" sce_sys/icon0.png
    FILE "${CMAKE_SOURCE_DIR}/packaging/vita/bg.png" sce_sys/livearea/contents/bg.png
    FILE "${CMAKE_SOURCE_DIR}/packaging/vita/startup.png" sce_sys/livearea/contents/startup.png
    FILE "${CMAKE_SOURCE_DIR}/packaging/vita/template.xml" sce_sys/livearea/contents/template.xml
    FILE "${CMAKE_BINARY_DIR}/packaging/vita-README.txt" README.txt
    FILE "${CMAKE_SOURCE_DIR}/LICENSE" LICENSE
    ${_VITA_DIAGNOSTIC_FILES})

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${LBA2_EXECUTABLE_NAME}-vita.vpk"
    DESTINATION . COMPONENT vita)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${LBA2_EXECUTABLE_NAME}.self"
    DESTINATION . RENAME eboot.bin COMPONENT vita)
install(FILES "${CMAKE_BINARY_DIR}/packaging/vita-title-id.txt"
    DESTINATION . RENAME title-id.txt COMPONENT vita)
install(FILES "$<TARGET_FILE:${LBA2_EXECUTABLE_NAME}>"
    DESTINATION debug RENAME "${LBA2_EXECUTABLE_NAME}.elf" COMPONENT vita)
