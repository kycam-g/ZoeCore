# ZoeCore Auto Learn module

CU_SET_PATH("CMAKE_MOD_AUTO_LEARN_ZOECORE_DIR" "${CMAKE_CURRENT_LIST_DIR}")

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_auto_learn_zoecore.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/loader.cpp")
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_auto_learn_zoecore.conf.dist")
