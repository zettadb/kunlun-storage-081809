INSTALL(CODE "execute_process(COMMAND bash post-install.sh ${CMAKE_INSTALL_PREFIX}
        WORKING_DIRECTORY ..)")
