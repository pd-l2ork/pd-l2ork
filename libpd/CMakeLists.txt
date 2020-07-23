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

set(ADAPTIVE_PATH ../externals/grh/adaptive/src)
file(GLOB PD_EXTRA_SOURCES_ADAPTIVE ${ADAPTIVE_PATH}/*.h ${ADAPTIVE_PATH}/*.c)

set(BSAYLOR_PATH ../externals/bsaylor)
file(GLOB PD_EXTRA_SOURCES_BSAYLOR ${BSAYLOR_PATH}/*.h ${BSAYLOR_PATH}/*.c)

set(CREB_PATH ../externals/creb/modules)
file(GLOB PD_EXTRA_SOURCES_CREB ${CREB_PATH}/*.h ${CREB_PATH}/*.c)

set(CXC_PATH ../externals/cxc)
file(GLOB PD_EXTRA_SOURCES_CXC ${CXC_PATH}/*.h ${CXC_PATH}/*.c)

set(DISIS_PATH ../externals/disis)
file(GLOB PD_EXTRA_SOURCES_DISIS ${DISIS_PATH}/*.h ${DISIS_PATH}/*.c)

set(EKEXT_PATH ../externals/ekext)
file(GLOB PD_EXTRA_SOURCES_EKEXT ${EKEXT_PATH}/*.h ${EKEXT_PATH}/*.c)

set(EXT13_PATH ../externals/ext13)
file(GLOB PD_EXTRA_SOURCES_EXT13 ${EXT13_PATH}/*.h ${EXT13_PATH}/*.c)

set(FFTEASE_PATH ../externals/fftease)
file(GLOB PD_EXTRA_SOURCES_FFTEASE ${FFTEASE_PATH}/*.h ${FFTEASE_PATH}/*.c)

set(GGEE1_PATH ../externals/ggee/control)
file(GLOB PD_EXTRA_SOURCES_GGEE1 ${GGEE1_PATH}/*.h ${GGEE1_PATH}/*.c)

set(GGEE2_PATH ../externals/ggee/experimental)
file(GLOB PD_EXTRA_SOURCES_GGEE2 ${GGEE2_PATH}/*.h ${GGEE2_PATH}/*.c)

set(GGEE3_PATH ../externals/ggee/filters)
file(GLOB PD_EXTRA_SOURCES_GGEE3 ${GGEE3_PATH}/*.h ${GGEE3_PATH}/*.c)

set(GGEE4_PATH ../externals/ggee/signal)
file(GLOB PD_EXTRA_SOURCES_GGEE4 ${GGEE4_PATH}/*.h ${GGEE4_PATH}/*.c)

set(HCS_PATH ../externals/hcs)
file(GLOB PD_EXTRA_SOURCES_HCS ${HCS_PATH}/*.h ${HCS_PATH}/*.c)

set(IEM1_PATH ../externals/iem/iem_adaptfilt/src)
file(GLOB PD_EXTRA_SOURCES_IEM1 ${IEM1_PATH}/*.h ${IEM1_PATH}/*.c)

set(IEM2_PATH ../externals/iem/iem_ambi/src)
file(GLOB PD_EXTRA_SOURCES_IEM2 ${IEM2_PATH}/*.h ${IEM2_PATH}/*.c)

set(IEM3_PATH ../externals/iem/iem_bin_ambi/src)
file(GLOB PD_EXTRA_SOURCES_IEM3 ${IEM3_PATH}/*.h ${IEM3_PATH}/*.c)

set(IEM4_PATH ../externals/iem/iem_delay/src)
file(GLOB PD_EXTRA_SOURCES_IEM4 ${IEM4_PATH}/*.h ${IEM4_PATH}/*.c)

set(IEM5_PATH ../externals/iem/iem_roomsim/src)
file(GLOB PD_EXTRA_SOURCES_IEM5 ${IEM5_PATH}/*.h ${IEM5_PATH}/*.c)

set(IEM6_PATH ../externals/iem/iem_spec2/src)
file(GLOB PD_EXTRA_SOURCES_IEM6 ${IEM6_PATH}/*.h ${IEM6_PATH}/*.c)

set(IEM7_PATH ../externals/iem/iem_tab/src)
file(GLOB PD_EXTRA_SOURCES_IEM7 ${IEM7_PATH}/*.h ${IEM7_PATH}/*.c)

set(IEM8_PATH ../externals/iem/iemguts/src)
file(GLOB PD_EXTRA_SOURCES_IEM8 ${IEM8_PATH}/*.h ${IEM8_PATH}/*.c)

set(IEM16_PATH ../externals/iem16/src)
file(GLOB PD_EXTRA_SOURCES_IEM16 ${IEM16_PATH}/*.h ${IEM16_PATH}/*.c)

set(IEMLIB1_PATH ../externals/iemlib/iemlib1/src)
file(GLOB PD_EXTRA_SOURCES_IEMLIB1 ${IEMLIB1_PATH}/*.h ${IEMLIB1_PATH}/*.c)

set(IEMLIB2_PATH ../externals/iemlib/iemlib2/src)
file(GLOB PD_EXTRA_SOURCES_IEMLIB2 ${IEMLIB2_PATH}/*.h ${IEMLIB2_PATH}/*.c)

set(MARKEX_PATH ../externals/markex)
file(GLOB PD_EXTRA_SOURCES_MARKEX ${MARKEX_PATH}/*.h ${MARKEX_PATH}/*.c)

set(MAXLIB_PATH ../externals/maxlib)
file(GLOB PD_EXTRA_SOURCES_MAXLIB ${MAXLIB_PATH}/*.h ${MAXLIB_PATH}/*.c)

set(MJLIB_PATH ../externals/mjlib)
file(GLOB PD_EXTRA_SOURCES_MJLIB ${MJLIB_PATH}/*.h ${MJLIB_PATH}/*.c)

set(MOONLIB_PATH ../externals/moonlib)
file(GLOB PD_EXTRA_SOURCES_MOONLIB ${MOONLIB_PATH}/*.h ${MOONLIB_PATH}/*.c)

set(MOTEX_PATH ../externals/motex)
file(GLOB PD_EXTRA_SOURCES_MOTEX ${MOTEX_PATH}/*.h ${MOTEX_PATH}/*.c)

set(MRPEACH_PATH1 ../externals/mrpeach/binfile)
file(GLOB PD_EXTRA_SOURCES_MRPEACH1 ${MRPEACH_PATH1}/*.h ${MRPEACH_PATH1}/*.c)

set(MRPEACH_PATH2 ../externals/mrpeach/cmos)
file(GLOB PD_EXTRA_SOURCES_MRPEACH2 ${MRPEACH_PATH2}/*.h ${MRPEACH_PATH2}/*.c)

set(MRPEACH_PATH3 ../externals/mrpeach/flist2tab)
file(GLOB PD_EXTRA_SOURCES_MRPEACH3 ${MRPEACH_PATH3}/*.h ${MRPEACH_PATH3}/*.c)

set(MRPEACH_PATH4 ../externals/mrpeach/life2x)
file(GLOB PD_EXTRA_SOURCES_MRPEACH4 ${MRPEACH_PATH4}/*.h ${MRPEACH_PATH4}/*.c)

set(MRPEACH_PATH5 ../externals/mrpeach/midifile)
file(GLOB PD_EXTRA_SOURCES_MRPEACH5 ${MRPEACH_PATH5}/*.h ${MRPEACH_PATH5}/*.c)

set(MRPEACH_PATH6 ../externals/mrpeach/net)
file(GLOB PD_EXTRA_SOURCES_MRPEACH6 ${MRPEACH_PATH6}/*.h ${MRPEACH_PATH6}/*.c)

set(MRPEACH_PATH7 ../externals/mrpeach/op~)
file(GLOB PD_EXTRA_SOURCES_MRPEACH7 ${MRPEACH_PATH7}/*.h ${MRPEACH_PATH7}/*.c)

set(MRPEACH_PATH8 ../externals/mrpeach/osc)
file(GLOB PD_EXTRA_SOURCES_MRPEACH8 ${MRPEACH_PATH8}/*.h ${MRPEACH_PATH8}/*.c)

set(MRPEACH_PATH9 ../externals/mrpeach/rc~)
file(GLOB PD_EXTRA_SOURCES_MRPEACH9 ${MRPEACH_PATH9}/*.h ${MRPEACH_PATH9}/*.c)

set(MRPEACH_PATH10 ../externals/mrpeach/rcosc~)
file(GLOB PD_EXTRA_SOURCES_MRPEACH10 ${MRPEACH_PATH10}/*.h ${MRPEACH_PATH10}/*.c)

set(MRPEACH_PATH11 ../externals/mrpeach/rojo~)
file(GLOB PD_EXTRA_SOURCES_MRPEACH11 ${MRPEACH_PATH11}/*.h ${MRPEACH_PATH11}/*.c)

set(MRPEACH_PATH12 ../externals/mrpeach/runningmean)
file(GLOB PD_EXTRA_SOURCES_MRPEACH12 ${MRPEACH_PATH12}/*.h ${MRPEACH_PATH12}/*.c)

set(MRPEACH_PATH13 ../externals/mrpeach/serializer)
file(GLOB PD_EXTRA_SOURCES_MRPEACH13 ${MRPEACH_PATH13}/*.h ${MRPEACH_PATH13}/*.c)

set(MRPEACH_PATH14 ../externals/mrpeach/slipdec)
file(GLOB PD_EXTRA_SOURCES_MRPEACH14 ${MRPEACH_PATH14}/*.h ${MRPEACH_PATH14}/*.c)

set(MRPEACH_PATH15 ../externals/mrpeach/slipenc)
file(GLOB PD_EXTRA_SOURCES_MRPEACH15 ${MRPEACH_PATH15}/*.h ${MRPEACH_PATH15}/*.c)

set(MRPEACH_PATH16 ../externals/mrpeach/sqosc~)
file(GLOB PD_EXTRA_SOURCES_MRPEACH16 ${MRPEACH_PATH16}/*.h ${MRPEACH_PATH16}/*.c)

set(MRPEACH_PATH17 ../externals/mrpeach/str)
file(GLOB PD_EXTRA_SOURCES_MRPEACH17 ${MRPEACH_PATH17}/*.h ${MRPEACH_PATH17}/*.c)

set(MRPEACH_PATH18 ../externals/mrpeach/tab2flist)
file(GLOB PD_EXTRA_SOURCES_MRPEACH18 ${MRPEACH_PATH18}/*.h ${MRPEACH_PATH18}/*.c)

set(MRPEACH_PATH19 ../externals/mrpeach/tabfind)
file(GLOB PD_EXTRA_SOURCES_MRPEACH19 ${MRPEACH_PATH19}/*.h ${MRPEACH_PATH19}/*.c)

set(MRPEACH_PATH20 ../externals/mrpeach/which)
file(GLOB PD_EXTRA_SOURCES_MRPEACH20 ${MRPEACH_PATH20}/*.h ${MRPEACH_PATH20}/*.c)

set(MRPEACH_PATH21 ../externals/mrpeach/xbee)
file(GLOB PD_EXTRA_SOURCES_MRPEACH21 ${MRPEACH_PATH21}/*.h ${MRPEACH_PATH21}/*.c)

set(PDDP_PATH ../externals/pddp)
file(GLOB PD_EXTRA_SOURCES_PDDP ${PDDP_PATH}/*.h ${PDDP_PATH}/*.c)

set(PLUGIN_PATH ../externals/plugin~)
file(GLOB PD_EXTRA_SOURCES_PLUGIN ${PLUGIN_PATH}/*.h ${PLUGIN_PATH}/*.c)

set(SIGPACK_PATH ../externals/sigpack)
file(GLOB PD_EXTRA_SOURCES_SIGPACK ${SIGPACK_PATH}/*.h ${SIGPACK_PATH}/*.c)

set(SMLIB_PATH ../externals/smlib)
file(GLOB PD_EXTRA_SOURCES_SMLIB ${SMLIB_PATH}/*.h ${SMLIB_PATH}/*.c)

set(TOF_PATH ../externals/tof/src)
file(GLOB PD_EXTRA_SOURCES_TOF ${TOF_PATH}/*.h ${TOF_PATH}/*.c)

set(VBAP_PATH ../externals/vbap)
file(GLOB PD_EXTRA_SOURCES_VBAP ${VBAP_PATH}/*.h ${VBAP_PATH}/*.c)

set(WINDOWING_PATH ../externals/windowing)
file(GLOB PD_EXTRA_SOURCES_WINDOWING ${WINDOWING_PATH}/*.h ${WINDOWING_PATH}/*.c)

set(ZEXY_PATH ../externals/zexy/src)
file(GLOB PD_EXTRA_SOURCES_ZEXY ${ZEXY_PATH}/*.h ${ZEXY_PATH}/*.c)

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
    ../externals/arraysize/arraysize.c
    ../externals/autotune/autotune~.c
    ../externals/bassemu~/bassemu~.c
    ${PD_EXTRA_SOURCES_ADAPTIVE}
    ${PD_EXTRA_SOURCES_BSAYLOR}
    ../externals/iem/comport/comport/comport.c
    ${PD_EXTRA_SOURCES_CREB}
    ${PD_EXTRA_SOURCES_CXC}
    ${PD_EXTRA_SOURCES_DISIS}
    ../externals/earplug~/earplug~.c
    ${PD_EXTRA_SOURCES_EKEXT}
    ${PD_EXTRA_SOURCES_EXT13}
    ${PD_EXTRA_SOURCES_FFTEASE}
    ../externals/footils/knob/knob.c
    ../externals/freeverb~/freeverb~.c
    ${PD_EXTRA_SOURCES_GGEE1}
    ${PD_EXTRA_SOURCES_GGEE2}
    ${PD_EXTRA_SOURCES_GGEE3}
    ${PD_EXTRA_SOURCES_GGEE4}
    ${PD_EXTRA_SOURCES_HCS}
    ${PD_EXTRA_SOURCES_IEM1}
    ${PD_EXTRA_SOURCES_IEM2}
    ${PD_EXTRA_SOURCES_IEM3}
    ${PD_EXTRA_SOURCES_IEM4}
    ${PD_EXTRA_SOURCES_IEM5}
    ${PD_EXTRA_SOURCES_IEM6}
    ${PD_EXTRA_SOURCES_IEM7}
    ${PD_EXTRA_SOURCES_IEM8}
    ${PD_EXTRA_SOURCES_IEM16}
    ${PD_EXTRA_SOURCES_IEMLIB1}
    ${PD_EXTRA_SOURCES_IEMLIB2}
    ../externals/jasch_lib/detox/detox.c
    ../externals/jasch_lib/memchr/memchr.c
    ../externals/jasch_lib/strchr/strchr.c
    ../externals/jasch_lib/strcut/strcut.c
    ../externals/jasch_lib/strlen/strlen.c
    ../externals/jasch_lib/strtok/strtok.c
    ../externals/jasch_lib/underscore/_.c
    ../externals/loaders/libdir/libdir.c
    ${PD_EXTRA_SOURCES_MARKEX}
    ${PD_EXTRA_SOURCES_MAXLIB}
    ${PD_EXTRA_SOURCES_MJLIB}
    ${PD_EXTRA_SOURCES_MOONLIB}
    ${PD_EXTRA_SOURCES_MOTEX}
    ${PD_EXTRA_SOURCES_MRPEACH1}
    ${PD_EXTRA_SOURCES_MRPEACH2}
    ${PD_EXTRA_SOURCES_MRPEACH3}
    ${PD_EXTRA_SOURCES_MRPEACH4}
    ${PD_EXTRA_SOURCES_MRPEACH5}
    ${PD_EXTRA_SOURCES_MRPEACH6}
    ${PD_EXTRA_SOURCES_MRPEACH7}
    ${PD_EXTRA_SOURCES_MRPEACH8}
    ${PD_EXTRA_SOURCES_MRPEACH9}
    ${PD_EXTRA_SOURCES_MRPEACH10}
    ${PD_EXTRA_SOURCES_MRPEACH11}
    ${PD_EXTRA_SOURCES_MRPEACH12}
    ${PD_EXTRA_SOURCES_MRPEACH13}
    ${PD_EXTRA_SOURCES_MRPEACH14}
    ${PD_EXTRA_SOURCES_MRPEACH15}
    ${PD_EXTRA_SOURCES_MRPEACH16}
    ${PD_EXTRA_SOURCES_MRPEACH17}
    ${PD_EXTRA_SOURCES_MRPEACH18}
    ${PD_EXTRA_SOURCES_MRPEACH19}
    ${PD_EXTRA_SOURCES_MRPEACH20}
    ${PD_EXTRA_SOURCES_MRPEACH21}
    ${PD_EXTRA_SOURCES_PDDP}
    ${PD_EXTRA_SOURCES_PLUGIN}
    ${PD_EXTRA_SOURCES_SIGPACK}
    ${PD_EXTRA_SOURCES_SMLIB}
    ${PD_EXTRA_SOURCES_TOF}
    ${PD_EXTRA_SOURCES_VBAP}
    ${PD_EXTRA_SOURCES_WINDOWING}
    ${PD_EXTRA_SOURCES_ZEXY}
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
