
# Copyright (c) 2012 Stefan Eilemann <eile@eyescale.ch>

# Legacy API definitions
set(LB_CO_DEFINES STDEXT_NAMESPACE_OPEN STDEXT_NAMESPACE_CLOSE)
set(LB_EQ_DEFINES MIN MAX MAX_UINT32 BIT1 BIT2 BIT3 BIT4 BIT5 BIT6 BIT7 BIT8
  BIT9 BIT10 BIT11 BIT12 BIT13 BIT14 BIT15 BIT16 BIT17 BIT18 BIT19 BIT20 BIT21
  BIT22 BIT23 BIT24 BIT25 BIT26 BIT27 BIT28 BIT29 BIT30 BIT31 BIT32 BIT33 BIT34
  BIT35 BIT36 BIT37 BIT38 BIT39 BIT40 BIT41 BIT42 BIT43 BIT44 BIT45 BIT46 BIT47
  BIT48 BIT_ALL_32 BIT_ALL_64 BIT_NONE
  TS_VAR TS_SCOPED TS_RESET TS_THREAD TS_NOT_THREAD
  ALIGN8 ALIGN16 TIMEOUT_INDEFINITE TIMEOUT_DEFAULT DLLIMPORT DLLEXPORT
  UNDEFINED_UINT32
  GCC_4_0_OR_LATER GCC_4_1_OR_LATER GCC_4_2_OR_LATER GCC_4_3_OR_LATER
  GCC_4_4_OR_LATER GCC_4_5_OR_LATER GCC_4_6_OR_LATER GCC_4_7_OR_LATER
  GCC_4_8_OR_LATER GCC_4_9_OR_LATER
  1KB 10KB 100KB 1MB 10MB 100MB 16KB 32KB 64KB 128KB 48MB 64MB
  ASSERT ASSERTINFO ERROR WARN INFO VERB CHECK UNIMPLEMENTED UNREACHABLE
  DONTCALL ABORT LOG SAFECAST)

# Compile definitions
set(EQUALIZER_DEFINES)

if(EQ_BIG_ENDIAN)
  list(APPEND EQUALIZER_DEFINES EQ_BIG_ENDIAN)
else()
  list(APPEND EQUALIZER_DEFINES EQ_LITTLE_ENDIAN)
endif()

if(EQUALIZER_BUILD_2_0_API)
  list(APPEND EQUALIZER_DEFINES EQ_2_0_API)
endif()

if(WIN32)
  set(ARCH Win32)
endif(WIN32)
if(APPLE)
  set(ARCH Darwin)
endif(APPLE)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(ARCH Linux)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

# Write defines file
set(DEFINES_FILE ${OUTPUT_INCLUDE_DIR}/eq/fabric/defines${ARCH}.h)
set(DEFINES_FILE_IN ${CMAKE_CURRENT_BINARY_DIR}/defines${ARCH}.h.in)

file(WRITE ${DEFINES_FILE_IN}
  "#ifndef EQFABRIC_DEFINES_${ARCH}_H\n"
  "#define EQFABRIC_DEFINES_${ARCH}_H\n\n"
  )

foreach(DEF ${EQUALIZER_DEFINES})
  file(APPEND ${DEFINES_FILE_IN}
    "#ifndef ${DEF}\n"
    "#  define ${DEF}\n"
    "#endif\n"
    )
endforeach(DEF ${EQUALIZER_DEFINES})

file(APPEND ${DEFINES_FILE_IN} "\n#ifndef EQ_2_0_API\n")
foreach(DEF ${LB_CO_DEFINES})
  file(APPEND ${DEFINES_FILE_IN}
    "#  ifdef LB_${DEF}\n"
    "#    define CO_${DEF} LB_${DEF}\n"
    "#  endif\n")
endforeach()
foreach(DEF ${LB_EQ_DEFINES})
  file(APPEND ${DEFINES_FILE_IN}
    "#  ifdef LB_${DEF}\n"
    "#    define EQ_${DEF} LB_${DEF}\n"
    "#  endif\n"
    "#  ifdef LB${DEF}\n"
    "#    define EQ${DEF} LB${DEF}\n"
    "#  endif\n")
endforeach()

file(APPEND ${DEFINES_FILE_IN}
  "#endif // EQ_2_0_API\n"
  "\n#endif /* EQFABRIC_DEFINES_${ARCH}_H */\n"
  )

update_file(${DEFINES_FILE_IN} ${DEFINES_FILE})
install(FILES ${DEFINES_FILE} DESTINATION include/eq/fabric COMPONENT dev)
