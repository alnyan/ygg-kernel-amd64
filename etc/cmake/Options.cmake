option(ENABLE_UNIX  "Enable support for Unix domain sockets"    ON)
option(ENABLE_NET   "Enable networking support"                 ON)

function(require what whom)
    if (NOT ${what})
        message(FATAL_ERROR "${what} is required by ${whom}")
    endif()
endfunction()

set(OPT_SRC "")

if (ENABLE_NET)
    set(OPT_SRC ${OPT_SRC}
                net/net.c
	            net/if.c
	            net/socket.c
	            net/util.c
	            net/ports.c
	            sys/sys_net.c)
endif()

if (ENABLE_UNIX)
    require(ENABLE_NET ENABLE_UNIX)

    set(OPT_SRC ${OPT_SRC} net/unix.c)
endif(ENABLE_UNIX)
