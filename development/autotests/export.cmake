# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.
#
# Copyright (c) 2012 Kornel Benko kornel@lyx.org
#
#
# LYX_ROOT  = ${TOP_SRC_DIR}/lib/{doc,examples,templates,tabletemplates}
# LYX_USERDIR_VER = Name of environment variable for the user directory
# lyx       =
# format    = lyx16x|lyx20x|lyx21x|lyx22x|lyx23x|xhtml|docbook5|epub
# extension = 16.lyx|20.lyx|21.lyx|22.lyx|23.lyx|xhtml|xml|epub
# file      = xxx
#
# Script should be called like:
# cmake -DLYX_ROOT=xxx \
#       -DLYX_TESTS_USERDIR=${LYX_TESTS_USERDIR} \
#       -DWORKDIR=${BUILD_DIR}/autotests/out-home \
#       -DLYX_USERDIR_VER=${LYX_USERDIR_VER} \
#       -Dlyx=xxx \
#       -Dformat=xxx \
#       -Dfonttype=xxx \
#       -Dextension=xxx \
#       -DLYX_FORMAT_NUM=${_lyx_format_num} \
#       -Dfile=xxx \
#       -Dinverted=[01] \
#       -DTOP_SRC_DIR=${TOP_SRC_DIR} \
#       -DIgnoreErrorMessage=(ON/OFF) \
#       -DPERL_EXECUTABLE=${PERL_EXECUTABLE} \
#       -DLYX_PYTHON_EXECUTABLE=${LYX_PYTHON_EXECUTABLE} \
#       -DXMLLINT_EXECUTABLE=${XMLLINT_EXECUTABLE} \
#       -DJAVA_EXECUTABLE=${JAVA_EXECUTABLE} \
#       -DENCODING=xxx \
#       -P "${TOP_SRC_DIR}/development/autotests/export.cmake"
#

set(_TestResultMessage "")
message(STATUS "IgnoreErrorMessage = \"${IgnoreErrorMessage}\"")
set(Perl_Script "${TOP_SRC_DIR}/development/autotests/useSystemFonts.pl")
set(Structure_Script "${TOP_SRC_DIR}/development/autotests/beginEndStructureCheck.pl")
set(readDefaultOutputFormat "${TOP_SRC_DIR}/development/autotests/readDefaultOutputFormat.pl")
set(LanguageFile "${TOP_SRC_DIR}/lib/languages")
set(GetTempDir "${TOP_SRC_DIR}/development/autotests/getTempDir.pl")
set(_ft ${fonttype})
execute_process(COMMAND ${PERL_EXECUTABLE} "${GetTempDir}" "${WORKDIR}" OUTPUT_VARIABLE TempDir)
message(STATUS "using fonttype = ${_ft}")
if(NOT ENCODING)
  set(ENCODING "default")
endif()
if(ENCODING STREQUAL "default")
  set(_enc)
else()
  set(_enc "_${ENCODING}")
endif()

# move the the last directory part of LYX_ROOT to filename
# to make the destination unique for otherwise identical
# filenames
get_filename_component(updir_ "${LYX_ROOT}" DIRECTORY)
get_filename_component(updir2_ "${LYX_ROOT}" NAME)
set(file "${updir2_}/${file}")
set(LYX_ROOT "${updir_}")
set(NO_FAILES 0)

if(format MATCHES "dvi|pdf")
  message(STATUS "LYX_TESTS_USERDIR = ${LYX_TESTS_USERDIR}")
  message(STATUS "Converting with perl ${Perl_Script}")
  set(LYX_SOURCE "${TempDir}/${file}_${format}_${_ft}${_enc}.lyx")
  message(STATUS "Using source \"${LYX_ROOT}/${file}.lyx\"")
  message(STATUS "Using dest \"${LYX_SOURCE}\"")
  if(NOT "${ENCODING}" STREQUAL "default")
    # message(STATUS "ENCODING = ${ENCODING}")
  endif()
  message(STATUS "Executing ${PERL_EXECUTABLE} \"${Perl_Script}\" \"${LYX_ROOT}/${file}.lyx\" \"${LYX_SOURCE}\" ${format} ${_ft} ${ENCODING} ${LanguageFile}")
  execute_process(COMMAND ${PERL_EXECUTABLE} "${Perl_Script}" "${LYX_ROOT}/${file}.lyx" "${LYX_SOURCE}" ${format} ${_ft} ${ENCODING} ${LanguageFile}
    RESULT_VARIABLE _err)
  string(COMPARE EQUAL  ${_err} 0 _erg)
  if(NOT _erg)
    set(NO_FAILES 1)
    message(FATAL_ERROR "Export failed while converting")
  endif()
  # We only need "_${ENCODING}" for unicode tests (because multiple encodings
  # are tested with the same format), but doesn't hurt to include for all.
  set(result_file_name ${file}_${_ft}_${ENCODING}.${extension})
else()
  message(STATUS "Converting with perl ${Perl_Script}")
  set(LYX_SOURCE "${TempDir}/${file}.lyx")
  message(STATUS "Using source \"${LYX_ROOT}/${file}.lyx\"")
  message(STATUS "Using dest \"${LYX_SOURCE}\"")
  execute_process(COMMAND ${PERL_EXECUTABLE} "${Perl_Script}" "${LYX_ROOT}/${file}.lyx" "${LYX_SOURCE}" ${format} "dontChange" "default" ${LanguageFile}
    RESULT_VARIABLE _err)
  string(COMPARE EQUAL  ${_err} 0 _erg)
  if(NOT _erg)
    set(NO_FAILES 1)
    message(FATAL_ERROR "Export failed while converting")
  endif()
  if(extension MATCHES "\\.lyx$")
    # Font-type not relevant for lyx16/lyx2[0123] exports
    set(result_file_base "${TempDir}/${file}")
  else()
    set(result_file_name ${file}.${extension})
  endif()
endif()

function(get_md5sum msource mresult mreserr)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E md5sum ${${msource}}
    OUTPUT_VARIABLE msource_md5sum_x
    RESULT_VARIABLE mres_err)
  if (NOT mres_err)
    string(REGEX REPLACE " .*" "" msource_md5sum ${msource_md5sum_x})
    set(${mresult} ${msource_md5sum} PARENT_SCOPE)
    message(STATUS "MD5SUM of \"${${msource}}\" is ${msource_md5sum}")
  else()
    set(${mresult} "xx" PARENT_SCOPE)
    message(STATUS "Error getting MD5SUM of \"${${msource}}\"")
  endif()
  set(${mreserr} ${mres_err} PARENT_SCOPE)
endfunction()

macro(Summary _err _msg)
  #message(STATUS "Summary called with err=${_err}, msg=${_msg}")
  if (_err)
    MATH(EXPR NO_FAILES "${NO_FAILES}+1")
    list(APPEND _TestResultMessage "Error: ${_msg}")
  else()
    list(APPEND _TestResultMessage "OK: ${_msg}")
  endif()
endmacro()

macro(validate_output output outputfile errmsg errx)
  if (NOT "${output}" STREQUAL "")
    file(WRITE "${outputfile}" "${output}")
    MATH(EXPR NO_FAILES "${NO_FAILES}+1")
    Summary(${errx} "Validating \"${outputfile}\", ${errmsg}")
  endif()
endmacro()

macro(check_xhtml_validate xhtml_file)
  message(STATUS "Calling ${LYX_PYTHON_EXECUTABLE} \"${TOP_SRC_DIR}/development/autotests/simplehtml_validity.py\" \"${TempDir}/${xhtml_file}\"")
  set(_outputfile "${TempDir}/${xhtml_file}.validate_out")
  set(_errorfile "${TempDir}/${xhtml_file}.validate_err")
  execute_process(
    COMMAND ${LYX_PYTHON_EXECUTABLE} "${TOP_SRC_DIR}/development/autotests/simplehtml_validity.py" "${TempDir}/${xhtml_file}"
    WORKING_DIRECTORY "${TempDir}"
    OUTPUT_VARIABLE xmlout
    ERROR_VARIABLE xmlerr
    RESULT_VARIABLE _err)
  if (NOT "${xmlout}" STREQUAL "")
    if ("${xmlout}" MATCHES "Error: ")
      validate_output("${xmlout}" "${_outputfile}" "found errors" -5)
    endif()
  endif()
  validate_output("${xmlerr}" "${_errorfile}" "Not emty" -6)
  if (NO_FAILES EQUAL 0 AND ${_err} EQUAL 254)
    # Ignore this result, validate the outputs only instead
    set(_err 0)
  endif()
  Summary(${_err} "Calling simplehtml_validity.py, result=${_err}")
endmacro()

macro(check_xhtml_xmllint xhtml_file)
  set(xmllint_params --loaddtd --noout)
  string(REPLACE ";" " " xmllint_params2 " ${xmllint_params}")
  message(STATUS "Calling: " ${XMLLINT_EXECUTABLE} ${xmllint_params2} " \"${TempDir}/${xhtml_file}\"")
  set(_outputfile "${TempDir}/${xhtml_file}.sax_out")
  execute_process(
    COMMAND ${XMLLINT_EXECUTABLE} ${xmllint_params} "${xhtml_file}"
    WORKING_DIRECTORY "${TempDir}"
    OUTPUT_VARIABLE xmlout
    ERROR_VARIABLE xmlerr
    RESULT_VARIABLE _err)
  file(WRITE "${_outputfile}" ${xmlout})
  Summary(${_err} "Checking \"${TempDir}/${xhtml_file}\" with ${XMLLINT_EXECUTABLE}")
  if (NOT _err)
    # check if parser output contains error messages
    message(STATUS "Check the output: ${PERL_EXECUTABLE} ${TOP_SRC_DIR}/development/autotests/examineXmllintOutput.pl")
    execute_process(
      COMMAND ${PERL_EXECUTABLE} "${TOP_SRC_DIR}/development/autotests/examineXmllintOutput.pl" "${_outputfile}"
      WORKING_DIRECTORY "${TempDir}"
      OUTPUT_VARIABLE xmlout
      RESULT_VARIABLE _err)
    Summary(${_err} "Parse messages of ${XMLLINT_EXECUTABLE} for errors")
  else()
    message(STATUS "Errors from xmllint: ${xmlerr}")
  endif()
  if (NOT _err)
    if (NOT "${xmlout}" STREQUAL "")
      message(STATUS "${xmlout}")
      set(_err -1)
      Summary(${_err} "Non empty output \"${_outputfile}\" of \"${XMLLINT_EXECUTABLE}\"")
    endif()
  endif()
endmacro()

macro(check_xhtml_xmlparser xhtml_file)
  message(STATUS "Calling ${PERL_EXECUTABLE} \"${TOP_SRC_DIR}/development/autotests/xmlParser.pl\" \"${xhtml_file}\"")
  execute_process(
    COMMAND ${PERL_EXECUTABLE} "${TOP_SRC_DIR}/development/autotests/xmlParser.pl" "${result_file_name}"
    WORKING_DIRECTORY "${TempDir}"
    OUTPUT_VARIABLE parserout
    ERROR_VARIABLE parsererr
    RESULT_VARIABLE _err
  )
  if (_err)
    message(STATUS "${parsererr}")
  endif()
  Summary(_err "Checking \"${TempDir}/${xhtml_file}\" with xmlParser.pl")
endmacro()

macro(check_docbook_jing xhtml_file)
  message(STATUS "Calling: ${JAVA_EXECUTABLE} -jar \"${TOP_SRC_DIR}/development/tools/jing.jar\" \"https://docbook.org/xml/5.2b09/rng/docbook.rng\" \"${TempDir}/${xhtml_file}\"")
  set(_outputfile "${TempDir}/${xhtml_file}.jing_out")
  execute_process(
    COMMAND ${JAVA_EXECUTABLE} -jar "${TOP_SRC_DIR}/development/tools/jing.jar" "https://docbook.org/xml/5.2b09/rng/docbook.rng" "${xhtml_file}"
    WORKING_DIRECTORY "${TempDir}"
    OUTPUT_VARIABLE jingout
    RESULT_VARIABLE _err)
  file(WRITE "${_outputfile}" ${jingout})
  message(STATUS "_err = ${_err}, jingout = ${jingout}")
  Summary(_err "Checking for empty output \"${_outputfile}\" of ${JAVA_EXECUTABLE} -jar \"${TOP_SRC_DIR}/development/tools/jing.jar\"")
endmacro()

set(ENV{${LYX_USERDIR_VER}} "${LYX_TESTS_USERDIR}")
set(ENV{LANG} "en_US.UTF-8") # to get all error-messages in english
set(ENV{LANGUAGE} "US:en")
#set(ENV{LC_ALL} "C")
if (extension MATCHES "\\.lyx$")
  include(${TOP_SRC_DIR}/development/autotests/CheckLoadErrors.cmake)
  get_md5sum(LYX_SOURCE source_md5sum _err)
  message(STATUS "Executing ${PERL_EXECUTABLE} ${readDefaultOutputFormat} ${LYX_SOURCE}")
  execute_process(
    COMMAND ${PERL_EXECUTABLE} ${readDefaultOutputFormat} "${LYX_SOURCE}"
    OUTPUT_VARIABLE _export_format)
  message(STATUS "readDefaultOutputFormat = ${_export_format}")
  if (${_export_format} MATCHES "pdf2")
    set(_texformat "pdflatex")
  elsif(${_export_format} MATCHES "pdf3")
    set(_texformat "platex")
  elsif(${_export_format} MATCHES "pdf4")
    set(_texformat "xetex")
  elsif(${_export_format} MATCHES "pdf5")
    set(_texformat "luatex")
  else()
    set(_texformat "empty")
  endif()

  foreach(_lv RANGE 1 20)
    set(used_tex_file "${result_file_base}.tex")
    set(result_file_base "${result_file_base}.${LYX_FORMAT_NUM}")
    set(result_file_name "${result_file_base}.lyx")
    file(REMOVE "${result_file_name}" "${result_file_name}.emergency" )
    message(STATUS "Executing ${lyx} -userdir \"${LYX_TESTS_USERDIR}\" -E ${format} ${result_file_name} \"${LYX_SOURCE}\"")
    message(STATUS "This implicitly checks load of ${LYX_SOURCE}")
    execute_process(
      COMMAND ${lyx} -userdir "${LYX_TESTS_USERDIR}" -E ${format} ${result_file_name} "${LYX_SOURCE}"
      RESULT_VARIABLE _err
      ERROR_VARIABLE lyxerr)
    Summary(_err "Converting \"${LYX_SOURCE}\" to format ${format}")
    if(_err)
      break()
    else()
      if(NOT EXISTS "${result_file_name}")
        set(_err -1)
        Summary(_err "Expected result file \"${result_file_name}\" does not exist")
        break()
      else()
        message(STATUS "Expected result file \"${result_file_name}\" exists")
        execute_process(
          COMMAND ${PERL_EXECUTABLE} ${Structure_Script} "${result_file_name}"
          RESULT_VARIABLE _err)
        Summary(_err "Structure of the intermediate file \"${result_file_name}\"")
        if(_err)
          break()
        endif()
        checkLoadErrors(lyxerr "${TOP_SRC_DIR}/development/autotests" _err)
        Summary(_err "Examination of error/warning messages of the conversion of \"${LYX_SOURCE}\" to format ${format}")
        if(_err)
          break()
        endif()
        message(STATUS "Create the corresponding .tex file \"${used_tex_file}\"")
       if (NOT ${_texformat} MATCHES "empty")
          execute_process(
            COMMAND ${lyx} -userdir "${LYX_TESTS_USERDIR}" -E ${_texformat} ${used_tex_file} "${LYX_SOURCE}"
            RESULT_VARIABLE _errx)
        endif()
      endif()
    endif()
    get_md5sum(result_file_name result_md5sum _err)
    Summary(_err "Getting md5sum of \"${result_file_name}\"")
    if(_err)
      # Somehow the created file is not readable?
      break()
    endif()
    # Check if result file identical to source file
    if(result_md5sum STREQUAL ${source_md5sum})
      if (format MATCHES "lyx(1[0-9]|2[01])x")
        # Do not compile, missing \origin statement prevents inclusion of
        # files with relative path
        message(STATUS "Not exporting due to missing \\origin statement")
        break()
      elseif(format MATCHES "lyx22x" AND file MATCHES "Minted")
        message(STATUS "Not exporting due to missing minted support")
        break()
      endif()
      message(STATUS "Source(${LYX_SOURCE}) and dest(${result_file_name}) are equal")
      message(STATUS "Now try to export the lyx2lyx created file")
      if (_export_format MATCHES "^pdf")
        message(STATUS "Executing ${lyx} -userdir \"${LYX_TESTS_USERDIR}\" -E ${_export_format} \"${result_file_name}.default\" \"${result_file_name}\"")
        execute_process(
          COMMAND ${lyx} -userdir "${LYX_TESTS_USERDIR}" -E ${_export_format} "${result_file_name}.default" "${result_file_name}"
          RESULT_VARIABLE _err
          ERROR_VARIABLE lyxerr)
        Summary(_err "Test-compilation of \"${result_file_name}\" to format ${_export_format}")
      endif()
      break()
    else()
      list(APPEND _TestResultMessage "Warning: \"${LYX_SOURCE}\" and \"${result_file_name}\" differ")
      message(STATUS "Source(${LYX_SOURCE}) and dest(${result_file_name}) are still different")
      if (_lv GREATER 10)
        set(_err 1)
        message(STATUS "Possible endless loop encountered")
        Summary(_err "Test-Loop exceeded the count of 10, Possible endless loop")
        break()
      endif()
    endif()
    set(source_md5sum ${result_md5sum})
    set(LYX_SOURCE ${result_file_name})
  endforeach()
else()
  if ($ENV{LYX_DEBUG_LATEX})
    set(LyXExtraParams -dbg latex)
  else()
    set(LyXExtraParams -dbg info)
  endif()
  if(IgnoreErrorMessage)
    foreach (_em ${IgnoreErrorMessage})
      list(APPEND LyXExtraParams --ignore-error-message ${_em})
    endforeach()
  endif()
  string(REGEX REPLACE ";" " " _LyXExtraParams "${LyXExtraParams}")
  message(STATUS "Executing in working dir ${TempDir}")
  message(STATUS "Executing ${lyx} ${_LyXExtraParams} -userdir \"${LYX_TESTS_USERDIR}\" -E ${format} ${result_file_name} \"${LYX_SOURCE}\"")
  file(REMOVE "${TempDir}/${result_file_name}")
  execute_process(
    COMMAND ${lyx} ${LyXExtraParams} -userdir "${LYX_TESTS_USERDIR}" -E ${format} ${result_file_name} "${LYX_SOURCE}"
    WORKING_DIRECTORY "${TempDir}"
    RESULT_VARIABLE _err)
  Summary(_err "Exporting \"${LYX_SOURCE}\" to format ${format}")
  if (NOT _err)
    #check if result file created
    if (NOT EXISTS "${TempDir}/${result_file_name}")
      message(STATUS "Expected result file \"${TempDir}/${result_file_name}\" does not exist")
      set(_err -1)
      Summary(_err "Expected result file \"${TempDir}/${result_file_name}\" does not exists")
    else()
      message(STATUS "Expected result file \"${TempDir}/${result_file_name}\" exists")
      if (extension MATCHES "^x(ht)?ml$")
        if (NOT format MATCHES "xhtml")
          # Check with perl xml-parser
          # needs XML::Parser module
          check_xhtml_xmlparser(${result_file_name})
        endif()
        if (JAVA_EXECUTABLE)
          if (format MATCHES "docbook5")
            check_docbook_jing(${result_file_name})
          else()
            #check_xhtml_validate(${result_file_name})
          endif()
        endif()
        if (XMLLINT_EXECUTABLE)
          check_xhtml_xmllint(${result_file_name})
        endif()
      endif()
    endif()
  endif()
endif()

if(inverted)
  string(COMPARE EQUAL  ${NO_FAILES} 0 _erg)
else()
  string(COMPARE NOTEQUAL  ${NO_FAILES} 0 _erg)
endif()

if ($ENV{LYX_DEBUG_LATEX})
  # Do not remove temporary files if one wants to examine them
  # for example if setting the env-var LYX_DEBUG_LATEX
  # This needs a remove all temp-dirs from time to time
  # $ cd build-dir
  # $ find autotests/out-home -name AbC_\* | xargs rm -rf
else()
  execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory "${TempDir}")
endif()
if(_TestResultMessage)
  message(STATUS "Msg Summary:")
  foreach(_l ${_TestResultMessage})
    message(STATUS "\t${_l}")
  endforeach()
endif()
if(_erg)
  message(STATUS "Exporting ${file}.lyx to ${format}")
  message(FATAL_ERROR "Export failed")
endif()
