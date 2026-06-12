# ZoeCore KillStreak module

CU_SET_PATH("CMAKE_MOD_ZOECORE_KILLSTREAK_DIR" "${CMAKE_CURRENT_LIST_DIR}")

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ZoeCoreKillStreak.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ZoeCoreKillStreak_loader.cpp")
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_killstreak.conf.dist")
