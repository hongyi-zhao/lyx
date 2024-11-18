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
