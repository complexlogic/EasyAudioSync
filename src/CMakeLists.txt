# C++ sources
set(CXX_SOURCES 
  main.cpp
  sync.cpp
  transcode.cpp
  metadata.cpp
  config.cpp
  util.cpp
  settings.cpp
  about.cpp
)
set(ICON_RESOURCE ${PROJECT_SOURCE_DIR}/assets/icons/icons.qrc)

# UI
set(UI_FILES 
  mainwindow.ui 
  settings.ui 
  settings_general.ui 
  settings_file_handling.ui 
  settings_transcoding.ui 
  settings_mp3.ui 
  settings_aac.ui 
  settings_ogg_vorbis.ui 
  settings_opus.ui 
  clean_dest_warning.ui 
  about.ui
)
foreach (FILE ${UI_FILES})
  set(UI_PATHS ${UI_PATHS} ${CMAKE_CURRENT_SOURCE_DIR}/ui/${FILE})
endforeach ()

# Translations
file(STRINGS "${PROJECT_SOURCE_DIR}/translations/languages.txt" LANGUAGES)
foreach(LANG ${LANGUAGES})
  list(GET LANG 0 CODE)
  list(GET LANG 1 NAME)
  list(APPEND TS_FILES "${PROJECT_SOURCE_DIR}/translations/${CODE}.ts")
  list(APPEND LANGS "    { \"${CODE}\", \"${NAME}\" },\n")
endforeach ()
list(LENGTH LANGS NB_LANGS)
string(REPLACE ";" "" LANG_PAIRS ${LANGS})
configure_file("${PROJECT_SOURCE_DIR}/translations/languages.hpp.in" "${PROJECT_BINARY_DIR}/languages.hpp")

# Make lupdate target for Qt 6
if (${QT_VERSION} VERSION_GREATER_EQUAL "6")
  qt_add_lupdate(${EXECUTABLE_NAME} 
    SOURCES
      ${UI_PATHS}
      ${CXX_SOURCES}
      ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/settings.hpp
    TS_FILES ${TS_FILES}
  )
endif ()
qt_add_translation(QM_FILES "${TS_FILES}")
foreach (FILE ${QM_FILES})
  get_filename_component(BASENAME ${FILE} NAME)
  string(APPEND TRANSLATION_FILES "    <file>${BASENAME}</file>\n")
endforeach ()
configure_file(${PROJECT_SOURCE_DIR}/translations/translations.qrc.in ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
set (SOURCE_FILES ${CXX_SOURCES} ${ICON_RESOURCE} ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
qt_wrap_ui(SOURCE_FILES ${UI_PATHS})
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
add_compile_definitions("BUILD_DATE=\"${BUILD_DATE}\"")
if (UNIX)
  if (APPLE)
    set(MACOS_ICON "${PROJECT_SOURCE_DIR}/assets/icons/${EXECUTABLE_NAME}.icns")
    set_source_files_properties("${MACOS_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND SOURCE_FILES "${MACOS_ICON}")
  endif ()
  add_executable(${EXECUTABLE_NAME} ${SOURCE_FILES})
  target_link_libraries(${EXECUTABLE_NAME}
    Qt${QT_VERSION}::Core
    Qt${QT_VERSION}::Widgets
    Qt${QT_VERSION}::Gui
    PkgConfig::LIBAVFORMAT
    PkgConfig::LIBAVCODEC
    PkgConfig::LIBSWRESAMPLE
    PkgConfig::LIBAVFILTER
    PkgConfig::LIBAVUTIL
    PkgConfig::FMT
    PkgConfig::SPDLOG
    PkgConfig::TAGLIB
    Threads::Threads
  )

  if (STRIP_BINARY)
    add_custom_command(TARGET ${EXECUTABLE_NAME}
      POST_BUILD
      COMMAND ${STRIP} "${PROJECT_BINARY_DIR}/${EXECUTABLE_NAME}"
    )
  endif ()
  if (APPLE)
    if (DMG)
      set_target_properties(${EXECUTABLE_NAME} PROPERTIES 
                            MACOSX_BUNDLE ON
                            MACOSX_BUNDLE_EXECUTABLE_NAME "${EXECUTABLE_NAME}"
                            MACOSX_BUNDLE_INFO_STRING "${PROJECT_DESCRIPTION}"
                            MACOSX_BUNDLE_ICON_FILE "${EXECUTABLE_NAME}.icns"
                            MACOSX_BUNDLE_GUI_IDENTIFIER "${RDNS_NAME}"
                            MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
                            MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
                            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
                            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
                            MACOSX_BUNDLE_COPYRIGHT "Public Domain"

      )
      install(CODE "include(BundleUtilities)\nfixup_bundle(\"${PROJECT_BINARY_DIR}/${EXECUTABLE_NAME}.app\" \"\" \"\")")
      add_custom_target(my_install COMMAND ${CMAKE_COMMAND} --build . --target install WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
      add_custom_target(dmg
        COMMAND mv "${EXECUTABLE_NAME}.app" "${PROJECT_NAME}.app" # fixup_bundle won't accept app names with spaces so need to manually rename
        COMMAND "${MACDEPLOYQT}" "${PROJECT_NAME}.app" -dmg
        COMMAND mv "${PROJECT_NAME}.dmg" "${EXECUTABLE_NAME}-${PROJECT_VERSION}-${CMAKE_OSX_ARCHITECTURES}.dmg"
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
      add_dependencies(dmg my_install)
    endif ()
  else ()
    install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
    set (DATA_DIR "${CMAKE_INSTALL_PREFIX}/share")
    install(FILES "${PROJECT_BINARY_DIR}/${RDNS_NAME}.desktop" DESTINATION "${DATA_DIR}/applications")
    foreach (ICON_SIZE 48 64 128 256)
      install(FILES "${PROJECT_SOURCE_DIR}/assets/icons/${EXECUTABLE_NAME}${ICON_SIZE}.png"
        DESTINATION "${DATA_DIR}/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps"
        RENAME "${RDNS_NAME}.png"
      )
    endforeach ()
    install (FILES "${PROJECT_SOURCE_DIR}/assets/icons/${EXECUTABLE_NAME}.svg" 
      DESTINATION "${DATA_DIR}/icons/hicolor/scalable/apps"
      RENAME "${RDNS_NAME}.svg"
    )

    if (PACKAGE)
      set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
      set(CPACK_PACKAGE_NAME ${EXECUTABLE_TITLE})
      set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
      set(CPACK_GENERATOR ${PACKAGE})
      if (PACKAGE STREQUAL "DEB")
        set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "qtbase5, libc6 (>=2.29), libstdc++6 (>=10.2)")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "complexlogic")
        set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
        set(CPACK_DEBIAN_ARCHIVE_TYPE "gnutar")
        set(CPACK_DEBIAN_COMPRESSION_TYPE "gzip")
        set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
      elseif (PACKAGE STREQUAL "RPM")
        set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
        set(CPACK_RPM_PACKAGE_LICENSE "BSD")
        set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
        set(CPACK_RPM_PACKAGE_AUTOREQPROV 0)
        set(CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase, glibc >= 2.37, libstdc++ >= 13.1")
      endif ()
      include(CPack)
    endif ()
  endif ()

# Windows build
elseif (WIN32)
  set(WINDOWS_RESOURCE "${PROJECT_SOURCE_DIR}/assets/icons/windows.rc")
  set(MANIFEST_FILE "${PROJECT_BINARY_DIR}/${EXECUTABLE_NAME}.manifest")
  add_executable(${EXECUTABLE_NAME} WIN32 ${SOURCE_FILES} ${MANIFEST_FILE} ${WINDOWS_RESOURCE})
  add_custom_command(TARGET ${EXECUTABLE_NAME}
    POST_BUILD
	COMMAND ${WINDEPLOYQT} --no-translations --no-network ${PROJECT_BINARY_DIR}/$<CONFIG>
  )
  target_compile_options(${EXECUTABLE_NAME} PUBLIC "/Zc:preprocessor")
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  target_include_directories(${EXECUTABLE_NAME} PUBLIC
    ${FFMPEG_INCLUDE_DIR}
    ${TAGLIB_INCLUDE_DIR}
  )
  target_link_libraries(${EXECUTABLE_NAME}
    Qt::Core
    Qt::Gui
    Qt::Widgets
    ${LIBAVFORMAT}
    ${LIBAVCODEC}
    ${LIBAVFILTER}
    ${LIBSWRESAMPLE}
    ${LIBAVUTIL}
    ${TAGLIB}
    spdlog::spdlog
  )

  install(DIRECTORY "${PROJECT_BINARY_DIR}/$<CONFIG>/" DESTINATION .)

  # Copy the Visual C++ runtime DLLs in case user doesn't have them installed
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  include(InstallRequiredSystemLibraries)
  foreach(required_lib vcruntime140.dll vcruntime140_1.dll msvcp140.dll msvcp140_1.dll msvcp140_2.dll)
    foreach(system_lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
      string(FIND ${system_lib} ${required_lib} found_lib)
      if (NOT found_lib EQUAL -1)
        install(FILES ${system_lib} DESTINATION .)
      endif ()
    endforeach ()
  endforeach ()
  if (NSIS_INSTALLER)
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${PROJECT_SOURCE_DIR}/assets/icons/${EXECUTABLE_NAME}.ico")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PROJECT_NAME}.lnk' '$INSTDIR\\\\${EXECUTABLE_NAME}.exe' ")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\${PROJECT_NAME}.lnk'")
    set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/assets/icons/${EXECUTABLE_NAME}.ico")
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
	  set(CPACK_NSIS_COMPRESSOR "lzma")
    set(CPACK_PACKAGE_FILE_NAME "${EXECUTABLE_NAME}-${PROJECT_VERSION}-setup")
  else ()
    set(CPACK_GENERATOR "ZIP")
	set(CPACK_PACKAGE_NAME ${EXECUTABLE_NAME})
  endif ()
  include(CPack)
endif ()
