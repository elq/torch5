# -*- cmake -*-
#
# MACRO_ADD_QTLUA_MODULE(<modulename> <...sourcefiles..>)
# Adds a target for a qtlua module.
# Links with the right libraries (lua,qtlua,qt4). 
# Adds the right include dirs and definitions.
# Declares the install rule for the target.

# MACRO_INSTALL_QTLUA_FILES(<modulename> <...luafiles..>)
# Install lua files for a module.


MACRO(MACRO_ADD_QTLUA_MODULE modulename)
  FIND_PACKAGE(Lua REQUIRED)
  FIND_PACKAGE(QtLua REQUIRED)
  
  ADD_DEFINITIONS(${QTLUA_DEFINITIONS} ${LUA_DEFINITIONS})
  INCLUDE_DIRECTORIES(${QTLUA_INCLUDE_DIR} ${LUA_INCLUDE_DIR})
  
  ADD_LIBRARY("${modulename}" MODULE ${ARGN})
  TARGET_LINK_LIBRARIES("${modulename}" ${QTLUA_LIBRARIES} ${LUA_LIBRARIES} ${QT_LIBRARIES})
  
  SET_TARGET_PROPERTIES("${modulename}" PROPERTIES PREFIX ""
    INSTALL_NAME_DIR "@executable_path/${Torch_INSTALL_BIN_RIDBUS}/${Torch_INSTALL_LUA_CPATH_SUBDIR}"
    INSTALL_RPATH "${Torch_INSTALL_LUA_CPATH};${Torch_INSTALL_LIB}"
    BUILD_WITH_INSTALL_RPATH ON )
  
  INSTALL(TARGETS "${modulename}" 
    RUNTIME DESTINATION ${Torch_INSTALL_LUA_CPATH_SUBDIR} 
    COMPONENT "${modulename}" 
    LIBRARY DESTINATION ${Torch_INSTALL_LUA_CPATH_SUBDIR}
    COMPONENT "${modulename}"  
    ARCHIVE DESTINATION ${Torch_INSTALL_LUA_CPATH_SUBDIR}
    COMPONENT "${modulename}" )
  
ENDMACRO(MACRO_ADD_QTLUA_MODULE modulename)


MACRO(MACRO_INSTALL_QTLUA_FILES modulename)
  INSTALL(FILES ${ARGN} 
    DESTINATION "${Torch_INSTALL_LUA_PATH_SUBDIR}/${modulename}"
    COMPONENT "${modulename}" )
ENDMACRO(MACRO_INSTALL_QTLUA_FILES modulename)


