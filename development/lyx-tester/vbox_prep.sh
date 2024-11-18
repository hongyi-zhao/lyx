#!/usr/bin/env bash

# This script is helpful for preparing a fresh installation of Ubuntu
# on a virtual box, whose only purpose is to run lyx-tester.

# Usage: ./vbox_prep.sh
# (no need to run with sudo)

set -e
set -u

ubuntu_ver="$( lsb_release -sr )"
ubuntu_ver_num="${ubuntu_ver/\./}"


# openssh-server: allows to ssh from your computer into the virtual box. This
#   is helpful for remotely ssh'ing into the host computer and then (thanks to
#   this line) ssh'ing into the virtual box.
# vim: useful to have a terminal editor when ssh'ing in
# git: probably already installed, unless downloading this script from browser
sudo apt-get -y install openssh-server vim git

# useful e.g. for the LyX keytests. Otherwise when going to screensave the keys
# are written to the password.
gsettings set org.gnome.desktop.screensaver lock-enabled false
gsettings set org.gnome.desktop.screensaver ubuntu-lock-on-suspend false
gsettings set org.gnome.desktop.session idle-delay 0

# Enable sources and install dependencies of "lyx" Ubuntu package.
#
# This chunk is shared by lyx-tester/lyx-tester, lyx-tester/vbox_prep.sh, master.sgk, vboxBigInstall.sh.
# Needed for "apt-get build-dep".
if [ ${ubuntu_ver_num} -ge 2404 ]; then
  # Don't really need to sub the entry for security, but easy to just keep as is.
  # use '$' so that this command can be run multiple times and will only make the sub once.
  sudo sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources
else
  sudo perl -pi -e 'next if /-backports/; s/^# (deb-src [^ ]+ [^ -]+ universe)$/$1/' /etc/apt/sources.list
fi
#
sudo apt-get --yes update
sudo apt-get -y build-dep lyx
