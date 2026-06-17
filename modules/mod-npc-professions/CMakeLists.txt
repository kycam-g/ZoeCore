# ZoeCore NPC Professions module

CU_SET_PATH("CMAKE_MOD_NPC_PROFESSIONS_DIR" "${CMAKE_CURRENT_LIST_DIR}")

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod_npc_professions.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/loader.cpp")
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_npc_professions.conf.dist")
