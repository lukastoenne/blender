# - Find Alembic library
# Find the native Alembic includes and library
# This module defines
#  ALEMBIC_INCLUDE_DIRS, where to find Alembic headers.
#  ALEMBIC_LIBRARIES, libraries to link against to use Alembic.
#  ALEMBIC_ROOT_DIR, The base directory to search for Alembic.
#                    This can also be an environment variable.
#  ALEMBIC_FOUND, If false, do not try to use Alembic.

#=============================================================================
# Copyright 2013 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

# If ALEMBIC_ROOT_DIR was defined in the environment, use it.
IF(NOT ALEMBIC_ROOT_DIR AND NOT $ENV{ALEMBIC_ROOT_DIR} STREQUAL "")
  SET(ALEMBIC_ROOT_DIR $ENV{ALEMBIC_ROOT_DIR})
ENDIF()

SET(_alembic_SEARCH_DIRS
  ${ALEMBIC_ROOT_DIR}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt/lib/alembic
)

SET(_alembic_FIND_COMPONENTS
  AlembicAbc
  AlembicAbcCoreAbstract
  AlembicAbcGeom
  AlembicAbcCoreHDF5
  AlembicAbcCoreOgawa
  AlembicOgawa
  AlembicUtil
)

FIND_PATH(_alembic_INCLUDE_DIRS
  NAMES
    Alembic/Abc/All.h
  HINTS
    ${_alembic_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

SET(_alembic_LIBRARIES)
FOREACH(COMPONENT ${_alembic_FIND_COMPONENTS})
  STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT)

  FIND_LIBRARY(ALEMBIC_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_alembic_SEARCH_DIRS}
    PATH_SUFFIXES
      lib/static
    )
  MARK_AS_ADVANCED(ALEMBIC_${UPPERCOMPONENT}_LIBRARY)
  LIST(APPEND _alembic_LIBRARIES "${ALEMBIC_${UPPERCOMPONENT}_LIBRARY}")
ENDFOREACH()

# handle the QUIETLY and REQUIRED arguments and set ALEMBIC_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Alembic DEFAULT_MSG
    _alembic_LIBRARIES _alembic_INCLUDE_DIRS)

IF(ALEMBIC_FOUND)
  SET(ALEMBIC_LIBRARIES ${_alembic_LIBRARIES})
  SET(ALEMBIC_INCLUDE_DIRS ${_alembic_INCLUDE_DIRS})
ENDIF(ALEMBIC_FOUND)

MARK_AS_ADVANCED(
  ALEMBIC_INCLUDE_DIRS
  ALEMBIC_LIBRARIES
)
