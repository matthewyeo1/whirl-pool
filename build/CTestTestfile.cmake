# CMake generated Testfile for 
# Source directory: C:/Users/user/whirl-pool
# Build directory: C:/Users/user/whirl-pool/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(whirlpool_tests "C:/Users/user/whirl-pool/build/Debug/tests.exe")
  set_tests_properties(whirlpool_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/user/whirl-pool/CMakeLists.txt;17;add_test;C:/Users/user/whirl-pool/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(whirlpool_tests "C:/Users/user/whirl-pool/build/Release/tests.exe")
  set_tests_properties(whirlpool_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/user/whirl-pool/CMakeLists.txt;17;add_test;C:/Users/user/whirl-pool/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(whirlpool_tests "C:/Users/user/whirl-pool/build/MinSizeRel/tests.exe")
  set_tests_properties(whirlpool_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/user/whirl-pool/CMakeLists.txt;17;add_test;C:/Users/user/whirl-pool/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(whirlpool_tests "C:/Users/user/whirl-pool/build/RelWithDebInfo/tests.exe")
  set_tests_properties(whirlpool_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/user/whirl-pool/CMakeLists.txt;17;add_test;C:/Users/user/whirl-pool/CMakeLists.txt;0;")
else()
  add_test(whirlpool_tests NOT_AVAILABLE)
endif()
