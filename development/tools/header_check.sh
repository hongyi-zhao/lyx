#!/usr/bin/env bash
# Syntax: header_check.sh [output_log_file]
#
# Typical usage: cd src; ../development/tools/header_check.sh ../development/tools/header_check.sh.log
# 
# The tree needs to be configured. ./configure --disable-debug --disable-optimization
# looks ~1/3 faster than -g -O2 alternatives.

# file header_check.sh
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author Scott Kostyshak

# Full author contact details are available in file CREDITS

# Description:

# All .cpp and .h files in the current directory and subdirectories
# are checked to see which include statements could be omitted without
# causing a build error. Many of these omissions would not be desired.
# For example, currently if you don't include Undo.h in Undo.cpp, there
# is no error because Undo.h is included in Cursor.h which is included
# in Undo.cpp. But clearly we do want to include Undo.h in Undo.cpp.

# The results are stored by default in header_check.sh.log

set -u

# Default
LOG_FILE="$(basename $0).log"
# User supplied
if ! [ a"$*" == a ]; then
  LOG_FILE="$1"
fi
echo Output file: "${LOG_FILE}"


#Now commented out, we use STDOUT for progress messages as well now
##Use log file above only if not redirected already from the command line
#OUTF=`ls /proc/$$/fd/1`
#OUTF_L=`readlink "$OUTF"`
##did user instructed to redirect to other file?
#if ! [[ a"${OUTF_L}" =~ a/dev/pts ]]; then
#  LOG_FILE="${OUTF_L}"
#else
#  # we are on console
#  echo Output file: "${LOG_FILE}"
#fi


# For only standard headers:
# PATTERN='^#include <'
# For all headers:
  PATTERN='^#include'

# Exclude common headers with regex
# (e.g. 'debug.h' will exclude 'support/debug.h')
# LyX was compiled on exotic environments and these sometimes
# require headers not needed on win/linux. So check the logs before
# deleting "redundant" standard libraries, Qt headers or includes around
# various ifdefs...
EXCLUDE='\(debug.h\|config.h\)'

NCORES=$(grep "CPU" /proc/cpuinfo | wc -l)

function BUILD_FN ()
{
	PREFIX=''

	# This is not a clean make.
	make -j${NCORES} 2>/dev/null 1>/dev/null
	ERROR_CODE=$?


	# The sed regex is more strict than it needs to be.
	if (( ERROR_CODE != 0 )); then
		# Use just one core, so we don't mix outputs
		IFS='' ERROR_OUTPUT=$(make 2>&1)
		# Without the grep, ERROR_OUTPUT might contain messages such as:
		# 2885 translated messages, 2169 fuzzy translations, 1356 untranslated messages.
		ERROR_OUTPUT=$(echo "${ERROR_OUTPUT}" | grep -i "error: ")
	
		cppORh=$(echo "${ERROR_OUTPUT}" | head -n 1 | \
			sed 's/.*\.\(cpp\|h\):[0-9]\+:[0-9]\+: error: .*/\1/')
		if [ "${cppORh}" = "cpp" ]; then
			PREFIX='suspicious: '
		elif [ "${cppORh}" != "h" ]; then
			echo -e "Warning: the error was not parsed correctly."\
				"\nThe following string was expected to be"\
				"'.cpp' or '.h': \n ${cppORh}" >&2
			echo ERROR_OUTPUT: "${ERROR_OUTPUT}"
			echo cppORh: "${cppORh}"
		fi
	fi
	return "${ERROR_CODE}"
}

echo Making the tree first...
make -j${NCORES} 2>&1 >/dev/null || exit

echo "BUILD_FN exited without error after removing the following include statements invididually:" > "${LOG_FILE}" \
|| { echo "ERROR: could not create log file, ${LOG_FILE}"; exit 1; }

( find -regex ".*\(h\)$" | sort ;  find -regex ".*\(cpp\)$" | sort ) | grep -vE "frontends/qt/ui_|frontends/qt/moc_" |
while read FILE_
do
	FILE_COPY=$( tempfile )
	cp "${FILE_}" "${FILE_COPY}" \
		|| { echo "ERROR: bu copy failed" >&2; exit 1; }
	echo -n "processing ${FILE_}..."
	grep "${PATTERN}" "${FILE_}" | \
	while read INCLUDE
	do
		echo -n ${INCLUDE},
		if echo "${INCLUDE}" | grep -q -v "${EXCLUDE}"; then
			cp "${FILE_COPY}" "${FILE_}" \
				|| { echo "ERROR: restore copy failed" >&2; exit 1; }
			sed -i "s@${INCLUDE}@@" "${FILE_}"

			BUILD_FN
			BUILD_FN_RET=$?
			if [ "${BUILD_FN_RET}" = 0 ]; then
				echo "${FILE_}::${INCLUDE}" >> "${LOG_FILE}"
			elif [ -n "${PREFIX}" ]; then
				if [ ${FILE_:(-2):2} == .h ]; then
					echo "${PREFIX}${FILE_}::${INCLUDE}" >> "${LOG_FILE}"
				fi
			fi
		fi
	done
	echo 
	cp "${FILE_COPY}" "${FILE_}"
done
