if(BRLCAD_ENABLE_QT)

  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${QTCAD_INCLUDE_DIRS}
    ${GED_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    )

endif(BRLCAD_ENABLE_QT)


# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(view_info_srcs
  view_info.cpp
  view_model.cpp
  )

set(view_info_moc_headers
  view_model.h
  )

if(BRLCAD_ENABLE_QT)

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(view_info_moc_srcs ${view_info_moc_headers})
    QT6_ADD_RESOURCES(icon_qrc icon.qrc)
  else()
    QT5_WRAP_CPP(view_info_moc_srcs ${view_info_moc_headers})
    QT5_ADD_RESOURCES(icon_qrc icon.qrc)
  endif(Qt6Widgets_FOUND)

  qged_plugin(qged_view_info SHARED ${view_info_srcs} ${view_info_moc_srcs} ${icon_qrc})
  if(Qt6Widgets_FOUND)
    target_link_libraries(qged_view_info libqtcad libged librt libbu Qt6::Widgets)
  else()
    target_link_libraries(qged_view_info libqtcad libged librt libbu Qt5::Widgets)
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_view_info APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_view_info view_info.cpp)
  PLUGIN_SETUP(qged_view_info qged)

  DISTCLEAN(
    icon.qrc.depends
    moc_view_model.cpp_parameters
    )

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${view_info_srcs}
  ${view_info_moc_headers}
  icon.qrc
  info.svg
  README
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

