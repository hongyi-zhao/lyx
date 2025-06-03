# This will match strings with Qt accelerators, like "so&me message",
# without having to specify the ampersand.
function git-grep-accel-robust ()
{
  # TODO preserve "git" args? i.e., allow user to not use -i, etc.
  #
  # Insert the string "&\?" after each character to allow for an ampersand
  # to be anywhere.
  # (https://unix.stackexchange.com/a/5981/197212)
  search_str="$( echo "$1" | sed 's/.\{1\}/&\&\\\?/g')"
  git grep -i "${search_str}"
}


# Change converters in the LyX preferences of the ctest build directory to use,
# e.g., "pdflatex-dev" (and friends) instead of "pdflatex". This helps us catch
# and report issues in LaTeX releases before the actual release.
# TODO: make inverse function also?
function ctest-lyx-prefs-inject-dev () {
  prefs_f='Testing/.lyx/preferences'
  # TODO: check if in the correct directory
  # TODO: check if file exists

  for i in "dvilualatex" "latex" "lualatex" "pdflatex" "uplatex" "xelatex"; do
    sed -i "s/${i} -shell-escape/${i}-dev -shell-escape/" "${prefs_f}"
  done
}
