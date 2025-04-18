# Finish the construction of the bundle
# by including the necessary QT libraries

set(QTPLUGINS "")

# Find a qt plugin and install it in the plugins directory
macro(install_qt_plugin _qt_plugin_name)
    get_target_property(qtlib "${_qt_plugin_name}" LOCATION)
    if(EXISTS ${qtlib})
        get_filename_component(qtdir ${qtlib} PATH)
        get_filename_component(qtdir ${qtdir} NAME)
        # Installing QT plugin ${qtlib} into ${qtplugin_dest_dir}/plugins/${qtdir}
        install(FILES "${qtlib}" DESTINATION ${qtplugin_dest_dir}/plugins/${qtdir} COMPONENT Runtime)
    else()
        message(FATAL_ERROR "Could not find QT plugin ${_qt_plugin_name}")
    endif()
endmacro()

if(LYX_BUNDLE)
    if(NOT APPLE)
        set(installed_lyx_path bin/${_lyx}${CMAKE_EXECUTABLE_SUFFIX})
        set(qtplugin_dest_dir bin)
        set(qt_conf_path bin/qt.conf)
    else()
        set(installed_lyx_path ${LYX_BUNDLE_NAME}.app)
        set(qtplugin_dest_dir "${LYX_BUNDLE_NAME}.app/Contents")
        set(qt_conf_path "${LYX_BUNDLE_NAME}.app/Contents/Resources/qt.conf")
    endif()

    if(${LYX_USE_QT} STREQUAL "QT5")
        set(QtScope "Qt5")
    elseif(${LYX_USE_QT} STREQUAL "QT6")
        set(QtScope "Qt6")
    endif()

    get_target_property( MyLoc "${QtScope}::QSvgPlugin" LOCATION)
    get_filename_component(MyDir ${MyLoc} PATH)
    set(QT_PLUGINS_DIR ${MyDir}/..)
    set(QT_LIBRARY_DIRS ${QT_PLUGINS_DIR}/../lib)

    file(GLOB QT_PLUGIN_DIRECTORIES "${QT_PLUGINS_DIR}/imageformats")
    install(DIRECTORY ${QT_PLUGIN_DIRECTORIES} DESTINATION "${qtplugin_dest_dir}/plugins/" COMPONENT Runtime REGEX "\\_debug\\.dylib$" EXCLUDE)

    if(APPLE)
      if(Qt6Core_FOUND OR (Qt5Core_FOUND AND (Qt5Core_VERSION VERSION_GREATER_EQUAL 5.10.0)))
          install_qt_plugin("${QtScope}::QMacStylePlugin")
       endif()
       install_qt_plugin("${QtScope}::QCocoaIntegrationPlugin")
    endif()

    if (APPLE AND LYX_USE_QT MATCHES "QT6")
      # With QT6, just copy all the plugins
      file(GLOB QT_PLUGIN_DIRECTORIES "${QT_PLUGINS_DIR}/*")
      install(DIRECTORY ${QT_PLUGIN_DIRECTORIES} DESTINATION "${qtplugin_dest_dir}/plugins/" COMPONENT Runtime)
    endif()
    # Install code does the following:
    # - Creates the qt.conf file
    # - install the platform specific plugins (with Qt5)
    # - Fixup the bundle
    install(CODE "include(BundleUtilities)
                    file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qt_conf_path}\" \"[Paths]\\rPlugins = PlugIns\\rTranslations = translations\")
                    file(GLOB_RECURSE QTPLUGINS
                    \"\${CMAKE_INSTALL_PREFIX}/${qtplugin_dest_dir}/plugins/*/*${CMAKE_SHARED_LIBRARY_SUFFIX}\")
                    message(STATUS \"QT plugins [\${CMAKE_INSTALL_PREFIX}/${qtplugin_dest_dir}/plugins/*/*${CMAKE_SHARED_LIBRARY_SUFFIX}]: \${QTPLUGINS}\")
                    fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${installed_lyx_path}\" \"\${QTPLUGINS}\" \"${QT_LIBRARY_DIRS}\")"
                    COMPONENT Runtime
            )

    if(APPLE)
        # fixup_bundle invalidates the codesign, so the app must be signed again.
        add_custom_target(sign_install WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}
          COMMAND /usr/bin/codesign --deep --force --sign "${CPACK_BUNDLE_APPLE_CERT_APP}" "LyX${LYX_VERSION}.app" VERBATIM)
    endif()

    if (APPLE AND LYX_DMG)
        # Setup the disk image layout
        install(CODE "
            message(STATUS \"Creating the folder view options (.DS_Store)\")
            execute_process(COMMAND /bin/ln -sf /Applications \"\${CMAKE_INSTALL_PREFIX}\")
            execute_process(COMMAND /bin/bash \"${CMAKE_CURRENT_SOURCE_DIR}/../../MacOSX/set_bundle_display_options.sh\"
            \"${CMAKE_BINARY_DIR}/ds_store\" \"${_lyx}\" \"${TOP_CMAKE_PATH}/../MacOSX/dmg-background.png\" 560 364)
        ")
    endif()
endif()
