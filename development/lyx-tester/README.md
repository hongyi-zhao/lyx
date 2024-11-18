# install-tl-ubuntu

## Usage

```s
sudo ./lyx-tester
```

## Description

These scripts help with testing LyX. The main idea is that
you can clone this repo into a fresh Ubuntu install (e.g. in a
virtual box) and run all LyX tests with just one command. This
script does the following:

- git clones and compiles the LyX master branch
- installs the dependencies needed to run the tests (e.g. R, knitr,
LilyPond, DocBook)
- installs language dependencies (Hebrew and Chinese fonts)
- runs install-tl-ubuntu (see below)
- runs all of the tests (tex2lyx, autotests, and export tests)

lyx-tester runs [install-tl-ubuntu](https://github.com/scottkosty/install-tl-ubuntu), which does the following:

- a full installation of the latest release of TeX Live.
- notifies apt so that apt does not try to install the Ubuntu
texlive-* packages as dependencies (e.g. if you do 'sudo apt-get
install lyx')
- installs (optionally) all of the LaTeX files that LyX templates and
examples depend on
- links to the folder where Ubuntu installs TeX files so that when you
install packages with LaTeX (e.g. FoilTeX and noweb), they will be
available.

Finally, the script ssh-lyx-tester dispatches lyx-tester to an
ssh-server. I use this with Amazon's EC2. All one has to do is run the
following
  ./ssh-lyx-tester <key-location> <server>

## Authors

Scott Kostyshak
Kornel Benko
