cmake_minimum_required (VERSION 2.8.11)
project (libpd C)

option(PD_UTILS  "Compile utilities" ON)
option(PD_EXTRA  "Compile extras" ON)
option(PD_MULTI  "Compile with multiple instance support" OFF)
option(PD_LOCALE "Set the LC_NUMERIC number format to the default C locale" ON)
option(PD_BUILD_C_EXAMPLES "Builds C API example programs" OFF)

if (MSVC)
    option(CMAKE_THREAD_LIBS_INIT "Path to pthreads library")
    option(PTHREADS_INCLUDE_DIR   "Path to the pthreads library header files")
    # We need pthreads.
    # Please provide the path to the pthreads library include path and
    # the path to the pthread library by specifying the following arguments
    # on the CMake command-line:
    # -DCMAKE_THREAD_LIBS_INIT:PATH=<path to library, either MSVC (for example pthreadVC2.lib) or GNUC version>
    # -DPTHREADS_INCLUDE_DIR:PATH=<path to the pthread header files>
    if (NOT CMAKE_THREAD_LIBS_INIT OR NOT PTHREADS_INCLUDE_DIR)
        message(FATAL_ERROR "Please provide a path to the pthreads library and its headers.")
    endif()
    # Prefer pthread implementation on platforms where multiple are available.
    set(CMAKE_THREAD_PREFER_PTHREAD ON)
endif()

if(WIN32)
    # Use Windows APIs compatible with most versions
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DWINVER=0x502 -DWIN32 -D_WIN32")
endif()
if (MSVC)
    set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DHAVE_STRUCT_TIMESPEC")
    add_definitions("/D_CRT_SECURE_NO_WARNINGS /wd4091 /wd4996")
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        # Select appropriate long int type on 64-bit Windows
        set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DPD_LONGINTTYPE=\"long long\"")
    endif()
else()
    set(CMAKE_C_FLAGS         "${CMAKE_C_FLAGS} -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-#warnings")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -ffast-math -funroll-loops -fomit-frame-pointer -O3")
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS} -g -O0")
    if (EMSCRIPTEN)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -s ASSERTIONS=2 -s SAFE_HEAP=1 -g4")
    endif()
    if(NOT APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
    endif()
endif()
add_definitions(-DPD=1 -DUSEAPI_DUMMY=1)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MACOSX_RPATH ON)

foreach (_BUILD_TYPE RELEASE DEBUG)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_BUILD_TYPE} ${PROJECT_SOURCE_DIR}/libs)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_BUILD_TYPE} ${PROJECT_SOURCE_DIR}/libs)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_BUILD_TYPE} ${PROJECT_SOURCE_DIR}/libs)
endforeach()

set(PD_SOURCES
    ../pd/src/config.h
    ../pd/src/d_arithmetic.c
    ../pd/src/d_array.c
    ../pd/src/d_ctl.c
    ../pd/src/d_dac.c
    ../pd/src/d_delay.c
    ../pd/src/d_fft.c
    ../pd/src/d_fft_fftsg.c
    ../pd/src/d_fftroutine.c
    ../pd/src/d_fftsg_h.c
    ../pd/src/d_filter.c
    ../pd/src/d_global.c
    ../pd/src/d_math.c
    ../pd/src/d_misc.c
    ../pd/src/d_osc.c
    ../pd/src/d_resample.c
    ../pd/src/d_soundfile.c
    ../pd/src/d_ugen.c
    ../pd/src/g_all_guis.c
    ../pd/src/g_all_guis.h
    ../pd/src/g_array.c
    ../pd/src/g_bang.c
    ../pd/src/g_canvas.c
    ../pd/src/g_canvas.h
    ../pd/src/g_clone.c
    ../pd/src/g_editor.c
    ../pd/src/g_graph.c
    ../pd/src/g_guiconnect.c
    ../pd/src/g_io.c
    ../pd/src/g_magicglass.c
    ../pd/src/g_magicglass.h
    ../pd/src/g_mycanvas.c
    ../pd/src/g_numbox.c
    ../pd/src/g_radio.c
    ../pd/src/g_readwrite.c
    ../pd/src/g_rtext.c
    ../pd/src/g_scalar.c
    ../pd/src/g_slider.c
    ../pd/src/g_template.c
    ../pd/src/g_text.c
    ../pd/src/g_toggle.c
    ../pd/src/g_traversal.c
    ../pd/src/g_undo.c
    ../pd/src/g_undo.h
    ../pd/src/g_vumeter.c
    ../pd/src/import.c
    ../pd/src/m_atom.c
    ../pd/src/m_binbuf.c
    ../pd/src/m_class.c
    ../pd/src/m_conf.c
    ../pd/src/m_glob.c
    ../pd/src/m_imp.h
    ../pd/src/m_memory.c
    ../pd/src/m_obj.c
    ../pd/src/m_pd.c
    ../pd/src/m_pd.h
    ../pd/src/m_sched.c
    ../pd/src/s_audio.c
    ../pd/src/s_audio_dummy.c
    ../pd/src/s_inter.c
    ../pd/src/s_loader.c
    ../pd/src/s_main.c
    ../pd/src/s_path.c
    ../pd/src/s_print.c
    ../pd/src/s_stuff.h
    ../pd/src/s_utf8.c
    ../pd/src/s_utf8.h
    ../pd/src/s_version.h
    ../pd/src/x_acoustics.c
    ../pd/src/x_arithmetic.c
    ../pd/src/x_array.c
    ../pd/src/x_connective.c
    ../pd/src/x_gui.c
    ../pd/src/x_interface.c
    ../pd/src/x_list.c
    ../pd/src/x_midi.c
    ../pd/src/x_misc.c
    ../pd/src/x_net.c
    ../pd/src/x_preset.c
    ../pd/src/x_preset.h
    ../pd/src/x_scalar.c
    ../pd/src/x_text.c
    ../pd/src/x_time.c
    ../pd/src/x_vexp.c
    ../pd/src/x_vexp.h
    ../pd/src/x_vexp_fun.c
    ../pd/src/x_vexp_if.c
)

set(PD_EXTRA_SOURCES
    ../pd/extra/bob~/bob~.c
    ../pd/extra/bonk~/bonk~.c
    ../pd/extra/choice/choice.c
    ../pd/extra/fiddle~/fiddle~.c
    ../pd/extra/loop~/loop~.c
    ../pd/extra/lrshift~/lrshift~.c
    ../pd/extra/pique/pique.c
    ../pd/extra/sigmund~/sigmund~.c
    ../pd/extra/stdout/stdout.c
    ../externals/freeverb~/freeverb~.c
    ../externals/sigpack/chop~.c
    ../externals/sigpack/decimate~.c
    ../externals/sigpack/diode~.c
    ../externals/sigpack/foldback~.c
    ../externals/sigpack/foldover~.c
    ../externals/sigpack/freqdiv~.c
    ../externals/sigpack/freqshift~.c
    ../externals/sigpack/hardlimit~.c
    ../externals/sigpack/harmgen~.c
    ../externals/sigpack/impulse~.c
    ../externals/sigpack/rectify~.c
    ../externals/sigpack/round~.c
    ../externals/sigpack/saturate~.c
    ../externals/sigpack/shape~.c
    ../externals/sigpack/sieve~.c
    ../externals/sigpack/sigpack.c
    ../externals/sigpack/split~.c
    ../externals/sigpack/transient~.c
    ../externals/sigpack/ustep~.c
    ../externals/sigpack/valverect~.c
    ../externals/sigpack/vowel~.c
    ../externals/sigpack/wavewrap~.c
    ../externals/zexy/src/0x2e.c
    ../externals/zexy/src/0x3c_tilde.c
    ../externals/zexy/src/0x3d0x3d_tilde.c
    ../externals/zexy/src/0x3e_tilde.c
    ../externals/zexy/src/0x7c0x7c_tilde.c
    ../externals/zexy/src/0x260x26_tilde.c
    ../externals/zexy/src/a2l.c
    ../externals/zexy/src/abs~.c
    ../externals/zexy/src/absgn~.c
    ../externals/zexy/src/atof.c
    ../externals/zexy/src/atoi.c
    ../externals/zexy/src/avg~.c
    ../externals/zexy/src/blockmirror~.c
    ../externals/zexy/src/blockshuffle~.c
    ../externals/zexy/src/blockswap~.c
    ../externals/zexy/src/date.c
    ../externals/zexy/src/demultiplex.c
    ../externals/zexy/src/demultiplex~.c
    ../externals/zexy/src/dfreq~.c
    ../externals/zexy/src/dirac~.c
    ../externals/zexy/src/doublepole~.c
    ../externals/zexy/src/drip.c
    ../externals/zexy/src/envrms~.c
    ../externals/zexy/src/fifop.c
    ../externals/zexy/src/freadln.c
    ../externals/zexy/src/fwriteln.c
    ../externals/zexy/src/glue.c
    ../externals/zexy/src/index.c
    ../externals/zexy/src/length.c
    ../externals/zexy/src/lifop.c
    ../externals/zexy/src/limiter~.c
    ../externals/zexy/src/list2int.c
    ../externals/zexy/src/list2lists.c
    ../externals/zexy/src/list2symbol.c
    ../externals/zexy/src/lister.c
    ../externals/zexy/src/listfind.c
    ../externals/zexy/src/liststorage.c
    ../externals/zexy/src/longload.c
    ../externals/zexy/src/lpt.c
    ../externals/zexy/src/makesymbol.c
    ../externals/zexy/src/matchbox.c
    ../externals/zexy/src/mavg.c
    ../externals/zexy/src/minmax.c
    ../externals/zexy/src/msgfile.c
    ../externals/zexy/src/multiline~.c
    ../externals/zexy/src/multiplex.c
    ../externals/zexy/src/multiplex~.c
    ../externals/zexy/src/multireceive.c
    ../externals/zexy/src/niagara.c
    ../externals/zexy/src/noish~.c
    ../externals/zexy/src/noisi~.c
    ../externals/zexy/src/operating_system.c
    ../externals/zexy/src/pack.c
    ../externals/zexy/src/pack~.c
    ../externals/zexy/src/packel.c
    ../externals/zexy/src/pdf~.c
    ../externals/zexy/src/prime.c
    ../externals/zexy/src/quantize~.c
    ../externals/zexy/src/rawprint.c
    ../externals/zexy/src/regex.c
    ../externals/zexy/src/relay.c
    ../externals/zexy/src/repack.c
    ../externals/zexy/src/repeat.c
    ../externals/zexy/src/route~.c
    ../externals/zexy/src/sfplay.c
    ../externals/zexy/src/sfrecord.c
    ../externals/zexy/src/sgn~.c
    ../externals/zexy/src/sigzero~.c
    ../externals/zexy/src/sleepgrain.c
    ../externals/zexy/src/sort.c
    ../externals/zexy/src/step~.c
    ../externals/zexy/src/strcmp.c
    ../externals/zexy/src/sum.c
    ../externals/zexy/src/swap~.c
    ../externals/zexy/src/symbol2list.c
    ../externals/zexy/src/tabdump.c
    ../externals/zexy/src/tabminmax.c
    ../externals/zexy/src/tabread4~~.c
    ../externals/zexy/src/tabset.c
    ../externals/zexy/src/tavg~.c
    ../externals/zexy/src/time.c
    ../externals/zexy/src/unpack.c
    ../externals/zexy/src/unpack~.c
    ../externals/zexy/src/urn.c
    ../externals/zexy/src/winNT_portio.c
    ../externals/zexy/src/wrap.c
    ../externals/zexy/src/z_zexy.c
    ../externals/zexy/src/z_zexy.h
    ../externals/zexy/src/z~.c
    ../externals/zexy/src/zexy.c
    ../externals/zexy/src/zexy.h
    ../externals/zexy/src/zexyconf.h
    ../externals/zexy/src/zexySIMD.h
)

set(LIBPD_SOURCES
    libpd_wrapper/s_libpdmidi.c
    libpd_wrapper/x_libpdreceive.c
    libpd_wrapper/x_libpdreceive.h
    libpd_wrapper/z_hooks.c
    libpd_wrapper/z_hooks.h
    libpd_wrapper/z_libpd.c
)
set(LIBPD_UTILS_SOURCES
    libpd_wrapper/util/ringbuffer.c
    libpd_wrapper/util/ringbuffer.h
    libpd_wrapper/util/z_print_util.c
    libpd_wrapper/util/z_print_util.h
    libpd_wrapper/util/z_queued.c
    libpd_wrapper/util/z_queued.h
)

source_group(pd         FILES ${PD_SOURCES})
source_group(pdextra    FILES ${PD_EXTRA_SOURCES})
source_group(libpd      FILES ${LIBPD_SOURCES})
source_group(libpdutils FILES ${LIBPD_UTILS_SOURCES})

include_directories(libpd_wrapper)
include_directories(../pd/src)

find_package(Threads REQUIRED)

# create final list of source files
set(SOURCE_FILES ${PD_SOURCES} ${LIBPD_SOURCES})
if(PD_UTILS)
    list(APPEND SOURCE_FILES ${LIBPD_UTILS_SOURCES})
endif()
if(PD_EXTRA)
    list(APPEND SOURCE_FILES ${PD_EXTRA_SOURCES})
endif()

add_library(libpd_static STATIC ${SOURCE_FILES})
add_library(libpd        SHARED ${SOURCE_FILES})

# set the output library name for libpd depending on the settings
set(LIBPD_OUTPUT_NAME     pd)
if (WIN32)
    set(LIBPD_OUTPUT_NAME libpd)
endif()
if (PD_MULTI)
    set(LIBPD_OUTPUT_NAME ${LIBPD_OUTPUT_NAME}-multi)
endif()
if (WIN32)
    set_target_properties(libpd_static PROPERTIES OUTPUT_NAME ${LIBPD_OUTPUT_NAME}-static)
else()
    set_target_properties(libpd_static PROPERTIES OUTPUT_NAME ${LIBPD_OUTPUT_NAME})
endif()
set_target_properties(libpd            PROPERTIES OUTPUT_NAME ${LIBPD_OUTPUT_NAME})

# add appropriate compile definitions
set(LIBPD_COMPILE_DEFINITIONS PD_INTERNAL)

if(NOT MSVC)
    list(APPEND LIBPD_COMPILE_DEFINITIONS HAVE_UNISTD_H=1)
endif()

if(PD_EXTRA)
    list(APPEND LIBPD_COMPILE_DEFINITIONS LIBPD_EXTRA=1)
endif()

if(PD_MULTI)
    list(APPEND LIBPD_COMPILE_DEFINITIONS PDINSTANCE=1 PDTHREADS=1)
endif()

if(NOT PD_LOCALE)
    list(APPEND LIBPD_COMPILE_DEFINITIONS LIBPD_NO_NUMERIC=1)
endif()

if(UNIX)
    list(APPEND LIBPD_COMPILE_DEFINITIONS HAVE_LIBDL)
    target_link_libraries(libpd PUBLIC ${CMAKE_DL_LIBS})
endif()

target_compile_definitions(libpd_static PRIVATE ${LIBPD_COMPILE_DEFINITIONS})
target_compile_definitions(libpd        PRIVATE ${LIBPD_COMPILE_DEFINITIONS})

# link against necessary libraries
if(MSVC)
    target_link_libraries(libpd PUBLIC Ws2_32)
    target_include_directories(libpd_static PUBLIC ${PTHREADS_INCLUDE_DIR})
    target_include_directories(libpd        PUBLIC ${PTHREADS_INCLUDE_DIR})
elseif(WIN32)
    target_link_libraries(libpd PUBLIC -Wl,--export-all-symbols ws2_32 kernel32 -static-libgcc)
endif()
target_link_libraries(libpd PUBLIC ${CMAKE_THREAD_LIBS_INIT})

if(NOT MSVC AND NOT APPLE AND NOT EMSCRIPTEN)
    find_library(MATH_LIB m)
    target_link_libraries(libpd PUBLIC ${MATH_LIB})
endif()

if(PD_BUILD_C_EXAMPLES)
    macro(ADD_EXAMPLE name)
        add_executable(${name} samples/c/${name}/${name}.c)
        target_link_libraries(${name} PUBLIC libpd)
        if(DEFINED CMAKE_DEBUG_POSTFIX)
            set_target_properties(${name} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
        endif()
    endmacro(ADD_EXAMPLE)

    ADD_EXAMPLE(pdtest)
    if(PD_MULTI)
        ADD_EXAMPLE(pdtest_multi)
        if(NOT MSVC)
            # uses gettimeofday()
            ADD_EXAMPLE(pdtest_gui)
        endif()
    endif()
    ADD_EXAMPLE(pdtest_thread)
endif()
