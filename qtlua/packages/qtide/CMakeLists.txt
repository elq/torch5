# -*- cmake -*-

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

# we need qt>=4.4 for this section
IF (QT4_FOUND AND WITH_QTLUA_IDE AND WITH_QTLUA)
  IF (QTVERSION AND NOT QT_VERSION_MAJOR)
    STRING(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*" 
      "\\1*10000+\\2*100+\\3" qt_version_expr "${QTVERSION}")
    MATH(EXPR QT_VERSION "${qt_version_expr}")
  ELSEIF (QT_VERSION_MAJOR)
    MATH(EXPR QT_VERSION
      "${QT_VERSION_MAJOR}*10000+${QT_VERSION_MINOR}*100+${QT_VERSION_PATCH}")
  ENDIF (QTVERSION AND NOT QT_VERSION_MAJOR)
  IF (NOT QT_VERSION OR QT_VERSION LESS "40400")
    MESSAGE(STATUS "Disabling package QTIDE (Needs Qt>=4.4.0)")
    SET(WITH_QTLUA_IDE FALSE)
  ENDIF (NOT QT_VERSION OR QT_VERSION LESS "40400")
ENDIF (QT4_FOUND AND WITH_QTLUA_IDE AND WITH_QTLUA)

# proceed
IF (WITH_QTLUA_IDE AND WITH_QTLUA)
  
  INCLUDE(${QT_USE_FILE})		     
  
  FIND_PACKAGE(QLua REQUIRED)
  INCLUDE_DIRECTORIES(${QLUA_INCLUDE_DIR})
  ADD_DEFINITIONS(${QLUA_DEFINITIONS})
  
  SET(qtide_SRC 
    qtide.h qtide.cpp qtide.qrc
    qluatextedit.h qluatextedit.cpp 
    qluagotodialog.ui qluafinddialog.ui qluareplacedialog.ui      
    qluaide.h qluaide.cpp
    qluamainwindow.h qluamainwindow.cpp
    qluaeditor.h qluaeditor.cpp
    qluasdimain.h qluasdimain.cpp
    qluamdimain.h qluamdimain.cpp
    qluamode.h qluamode.cpp
    qluamode_txt.cpp 
    qluamode_hlp.cpp 
    qluamode_lua.cpp 
    qluamode_c.cpp )
  
  MACRO_QT4_AUTOGEN(qtide_GEN ${qtide_SRC})
  MACRO_ADD_QTLUA_MODULE(libqtide ${qtide_SRC} ${qtide_GEN})
  TARGET_LINK_LIBRARIES(libqtide ${QLUA_LIBRARIES})
  
  MACRO_INSTALL_QTLUA_FILES(qtide init.lua)
  ADD_TORCH_HELP(qtide "QT Interface")
  
ENDIF(WITH_QTLUA_IDE AND WITH_QTLUA)
    
