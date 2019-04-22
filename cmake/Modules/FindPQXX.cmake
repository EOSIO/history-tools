#[=======================================================================[.rst:
FindPQXX
-----------------------------

Find PostGreSQL C++ library and header file
Sets
  PQXX_FOUND                to 0 or 1 depending on result
  PQXX_INCLUDE_DIR          to the pqxx include path
  PQXX_INCLUDE_DIRECTORIES  to the pqxx include path
  PQXX_LIBRARY              to the pqxx client library
  PQXX_LIBRARIES            to the pqxx client library and required dependencies
Sample Usage:

  find_package(PQXX)
  find_package(PQXX 6.3)
#]=======================================================================]

include(FindPackageHandleStandardArgs)

set(PostgreSQL_TYPE_INCLUDE_DIR /usr/include/postgresql)
if(PQXX_FIND_REQUIRED)
  find_package( PostgreSQL REQUIRED )
else()
  find_package( PostgreSQL )
endif()
if ( PostgreSQL_FOUND )
  file( TO_CMAKE_PATH "$ENV{PQXX_DIR}" _PQXX_DIR )

  find_library( PQXX_LIBRARY
    NAMES libpqxx pqxx
    HINTS ENV PQXX_DIR
    PATHS
      ${_PQXX_DIR}/lib
      ${CMAKE_INSTALL_PREFIX}/lib
      ${CMAKE_INSTALL_PREFIX}/bin
      /usr/local/pgsql/lib
      /usr/local/lib
      /usr/lib
    PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE}
    DOC "Location of libpqxx library"
    NO_DEFAULT_PATH
  )

  find_path( PQXX_INCLUDE_DIR
    NAMES pqxx
    HINTS ENV PQXX_DIR
    PATHS
      ${_PQXX_DIR}/include
      ${CMAKE_INSTALL_PREFIX}/include
      /usr/local/pgsql/include
      /usr/local/include
      /usr/include
    DOC "Path to pqxx/pqxx header file. Do not include the 'pqxx' directory in this value."
    NO_DEFAULT_PATH
  )
endif ( PostgreSQL_FOUND )

if( PQXX_INCLUDE_DIR )
  file(STRINGS ${PQXX_INCLUDE_DIR}/pqxx/version.hxx PQXX_VERSION_DEFINE REGEX "PQXX_VERSION ")
  string(REGEX MATCH "[0-9]+\.[0-9]+\.[0-9]+" PQXX_VERSION ${PQXX_VERSION_DEFINE})
else()
  set(PQXX_VERSION "")
endif()

find_package_handle_standard_args(PQXX REQUIRED_VARS PQXX_LIBRARY PQXX_INCLUDE_DIR
                                  VERSION_VAR PQXX_VERSION)

set( PQXX_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIR};${PostgreSQL_INCLUDE_DIR}" CACHE STRING "Include directories for PostGreSQL C++ library"  FORCE )
set( PQXX_LIBRARIES "${PQXX_LIBRARY};${PostgreSQL_LIBRARY}" CACHE STRING "Link libraries for PostGreSQL C++ interface" FORCE )

mark_as_advanced(FORCE PQXX_INCLUDE_DIRECTORIES)
mark_as_advanced(FORCE PQXX_LIBRARIES)
