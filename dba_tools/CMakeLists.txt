INSTALL(CODE "execute_process(COMMAND bash post-install.sh ${CMAKE_INSTALL_PREFIX} ${CMAKE_BUILD_TYPE}
        WORKING_DIRECTORY ..)")
