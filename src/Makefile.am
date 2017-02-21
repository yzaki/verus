bin_PROGRAMS = verus_client verus_server
verus_client_SOURCES = verus_client.cpp verus.hpp
verus_server_SOURCES = verus_server.cpp verus.hpp
AM_CXXFLAGS = -std=c++11 -w $(BOOST_CPPFLAGS)
ACLOCAL_AMFLAGS = -I m4

verus_client_LDADD = -lpthread $(BOOST_SYSTEM_LIB)

if BUILD_OS_IS_DARWIN
verus_server_LDADD = -lpthread -lalglib $(BOOST_SYSTEM_LIB)
else
verus_server_LDADD = -lpthread -lalglib $(BOOST_SYSTEM_LIB)
endif
