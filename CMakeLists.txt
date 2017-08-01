# Copyright 2011,2012 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS} /opt/local/lib)

list(APPEND gsm_packer_sources
    stream2burst_impl.cc
    cyclic_buffer.cc
    buffer_delegate_impl.cc
    gsm_preprocessor.cc
    decoder/sch.c
    decoder/gsmstack.c
    decoder/cch.c
    decoder/fire_crc.c
    decoder/interleave.c
)

set(gsm_packer_sources "${gsm_packer_sources}" PARENT_SCOPE)
if(NOT gsm_packer_sources)
	MESSAGE(STATUS "No C++ sources... skipping lib/")
	return()
endif(NOT gsm_packer_sources)

add_library(gnuradio-gsm_packer SHARED ${gsm_packer_sources})
target_link_libraries(gnuradio-gsm_packer osmocore ${Boost_LIBRARIES} ${GNURADIO_ALL_LIBRARIES})
set_target_properties(gnuradio-gsm_packer PROPERTIES DEFINE_SYMBOL "gnuradio_gsm_packer_EXPORTS")

if(APPLE)
    set_target_properties(gnuradio-gsm_packer PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
    )
endif(APPLE)

########################################################################
# Install built library files
########################################################################
install(TARGETS gnuradio-gsm_packer
    LIBRARY DESTINATION lib${LIB_SUFFIX} # .so/.dylib file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
    RUNTIME DESTINATION bin              # .dll file
)

########################################################################
# Build and register unit test
########################################################################
include(GrTest)

include_directories(${CPPUNIT_INCLUDE_DIRS})

list(APPEND test_gsm_packer_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/test_gsm_packer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_gsm_packer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_stream2burst.cc
)

add_executable(test-gsm_packer ${test_gsm_packer_sources})

target_link_libraries(
  test-gsm_packer
  ${GNURADIO_RUNTIME_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CPPUNIT_LIBRARIES}
  gnuradio-gsm_packer
)

GR_ADD_TEST(test_gsm_packer test-gsm_packer)
