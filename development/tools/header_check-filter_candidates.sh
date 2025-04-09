#!/bin/bash

# Narrow down possible candidates for include removal in .cpp files.
# The main idea here is that the removal candidates in header_check.sh.log
# are real only when included in the translation unit just once.
# If included twice or more the removal candidacy is just effect of indirect
# include from other header.

# To detect this we just insert unconditionally unique declaration ("trap") 
# into each header file and look on the result of preprocessor, which will
# be stored now stored in .o (instead of usual compiled binary content).
# Output are only the cases where trap is found exactly once in a given .o.

# To be run on the results of header_check.sh script.
# Careful, the tree will be left in mess after running this
# (changed header and (git untracked) Makefiles...).


# we want to be in lyx/src/
if ! [ -e LyX.cpp ]; then echo Wrong dir; exit 1; fi

find . -name "*.h" -print0 | while IFS= read -r -d $'\0' file; do
  filename=$(basename "$file" .h)
  class_declaration="class tt${filename}_h;"
  # Read the entire content of the file
  content=$(cat "$file")
  echo "$class_declaration" > "$file"
  echo "$content" >> "$file"
  #echo "Processed: $file"
done

# Let's have all .o file contain just preprocessed output
find . -name "Makefile" -print0 | xargs -0 sed -i 's/^CXX = g++ *$/CXX = g++ -E /'
make -k 2>/dev/null 1>/dev/null

input_file="../development/tools/header_check.sh.log"

if [ -z "$input_file" ]; then  exit 1 ; fi

cat "$input_file" | grep .cpp | grep -v filtered: |
while IFS='::' read -r header include; do
  if [ -n "$header" ] && [ -n "$include" ]; then
    header_file=$(basename "$header")
    header_file="${header_file%.*}" # Extract filename without extension

    # Remove the "#include" part and the quotes and alike
    trap_file=$(echo "$include" | tr '<' '"' | tr '>' '"' |sed -E 's/.*#include\s+"(.*)"/\1/'|sed 's/.*#include.//'| sed 's/\/\/.*//'|sed 's/ $//' |sed 's/.h$/_h/'|sed 's/.hxx$/_h/'|sed 's/.*\///')
    trap_declaration="tt$trap_file"

    preprocessed_output="${header%.cpp}.o"

    #echo "Processing header: $header"
    #echo "Preprocess output: $preprocessed_output"
    #echo "Include statement: $include"
    #echo "Expected trap: $trap_declaration"

    if [ -e "$preprocessed_output" ]; then
      trap_count=$(cat "$preprocessed_output" | grep -F -c "$trap_declaration")

      #echo "Trap declaration found $trap_count times in preprocessed output."

      if [ 1 -eq "$trap_count" ]; then
        echo filtered: $header : "${trap_declaration#tt}" # found $trap_count==1 times in $header."
      fi

    else
      echo filtered: miss: "$preprocessed_output" # The corresponding .cpp will not be checked for candidates.
    fi

  else
    echo "WARNING: Skipping invalid line."
  fi
done | sort | uniq
