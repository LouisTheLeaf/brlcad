set(AP214_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../g-step
  ${STEPCODE_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIRS}
  )

link_directories(${CMAKE_BINARY_DIR}/${LIB_DIR})

BRLCAD_INCLUDE_DIRS(AP214_INCLUDE_DIRS)

set(STEP_SCHEMA_FILE ${CMAKE_SOURCE_DIR}/src/other/ext/stepcode/data/ap214e3/AP214E3_2010.exp)
add_definitions(-DAP214e3)

GENERATE_SCHEMA_INPUTS(${STEP_SCHEMA_FILE} gap214)

set(gap214_SOURCES
  ${express_srcs}
  ../BRLCADWrapper.cpp
  ../g-step/AP203.cpp
  ../g-step/Assembly_Product.cpp
  ../g-step/Default_Geometric_Context.cpp
  ../g-step/ON_Brep.cpp
  ../g-step/ON_NurbsCurve.cpp
  ../g-step/ON_NurbsSurface.cpp
  ../g-step/Shape_Representation.cpp
  ../g-step/Shape_Representation_Relationship.cpp
  Shape_Definition_Representation.cpp
  Add_Tree.cpp
  Comb.cpp
  G_Objects.cpp
  g-ap214.cpp
  )

set(gap214_LIBS
  libwdb
  librt
  libbrep
  libbn
  libbu
  steputils
  stepeditor
  stepdai
  stepcore
  ${OPENNURBS_LIBRARIES}
  )
list(APPEND gap214_LIBS ${gap214_LIBS})

BRLCAD_ADDEXEC(g-ap214 "${gap214_SOURCES}" "${gap214_LIBS}" NO_STRICT FOLDER Conv)
add_dependencies(g-ap214 step-express-${SCHEMA_NAME}-gap214)

if (HIDE_INTERNAL_SYMBOLS)
  set(gap214_IMPORTS
    BU_DLL_IMPORTS
    BN_DLL_IMPORTS
    RT_DLL_IMPORTS
    WDB_DLL_IMPORTS
    SC_CORE_DLL_IMPORTS
    SC_DAI_DLL_IMPORTS
    SC_EDITOR_DLL_IMPORTS
    SC_UTILS_DLL_IMPORTS
    )
  list(APPEND gap214_IMPORTS ${gap214_IMPORTS})
  set_property(TARGET g-ap214 APPEND PROPERTY COMPILE_DEFINITIONS "${gap214_IMPORTS}")
  set_property(SOURCE
    ${CMAKE_CURRENT_BINARY_DIR}/AUTOMOTIVE_DESIGN/SdaiAUTOMOTIVE_DESIGN.cc
    APPEND PROPERTY COMPILE_FLAGS "/bigobj")
endif (HIDE_INTERNAL_SYMBOLS)

set(IGNORE_FILES
  CMakeLists.txt
  AP214e3.h
  )
CMAKEFILES(${IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
