#! /usr/bin/env perl
# -*- mode: perl; -*-
#
# file search_url.pl
# script to search for url's in lyxfiles
# and testing their validity.
#
# Syntax: search_url.pl [(filesToScan|(ignored|reverted|extra|selected)URLS)={path_to_control]*
# Param value is a path to a file containing list of xxx:
# filesToScan={xxx = lyx-file-names to be scanned for}
# ignoredURLS={xxx = urls that are discarded from test}
# revertedURLS={xxx = urls that should fail, to test the test with invalid urls}
# extraURLS={xxx = urls which should be also checked}
#
# This file is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this software; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Copyright (c) 2013 Kornel Benko <kornel@lyx.org>
#           (c) 2013 Scott Kostyshak <skotysh@lyx.org>

use strict;
use warnings;

BEGIN {
  use File::Spec;
  my $p = File::Spec->rel2abs(__FILE__);
  $p =~ s/[\/\\]?[^\/\\]+$//;
  unshift(@INC, "$p");
}

use Cwd qw(abs_path);
use CheckURL;
use Try::Tiny;
use locale;
use POSIX qw(locale_h);
use Readonly;

binmode(STDOUT, ":encoding(UTF-8)");

Readonly::Scalar my $NR_JOBS => 10;

setlocale(LC_CTYPE,    "");
setlocale(LC_MESSAGES, "en_US.UTF-8");

use File::Temp qw/ tempfile tempdir /;
use File::Spec;
use Fcntl qw(:flock SEEK_END);

# Prototypes
sub printNotUsedURLS($\%);
sub replaceSpecialChar($);
sub readUrls($\%);
sub parse_file($ );
sub handle_url($$$ );
sub printx($$$$);
sub getnrjobs($$$);
##########

my %URLS                = ();
my %ignoredURLS         = ();
my %revertedURLS        = ();
my %extraURLS           = ();
my %selectedURLS        = ();
my %knownToRegisterURLS = ();
my %extraTestURLS       = ();
my $summaryFile         = undef;

my $checkSelectedOnly = 0;
for my $arg (@ARGV) {
  die("Bad argument \"$arg\"") if ($arg !~ /=/);
  my ($type, $val) = split("=", $arg);
  if ($type eq "filesToScan") {

    #The file should be a list of files to search in
    if (open(FLIST, '<', $val)) {
      while (my $l = <FLIST>) {
        chomp($l);
        parse_file($l);
      }
      close(FLIST);
    }
  }
  elsif ($type eq "ignoredURLS") {
    readUrls($val, %ignoredURLS);
  }
  elsif ($type eq "revertedURLS") {
    readUrls($val, %revertedURLS);
  }
  elsif ($type eq "extraURLS") {
    readUrls($val, %extraURLS);
  }
  elsif ($type eq "selectedURLS") {
    $checkSelectedOnly = 1;
    readUrls($val, %selectedURLS);
  }
  elsif ($type eq "knownToRegisterURLS") {
    readUrls($val, %knownToRegisterURLS);
  }
  elsif ($type eq "summaryFile") {
    if (open(SFO, '>:encoding(UTF8)', "$val")) {
      $summaryFile = $val;
    }
  }
  else {
    die("Invalid argument \"$arg\"");
  }
}

my @urls     = sort keys %URLS, keys %extraURLS;
my @testvals = ();

# Tests
#my @urls = ("ftp://ftp.edpsciences.org/pub/aa/readme.html", "ftp://ftp.springer.de/pub/tex/latex/compsc/proc/author");
my $errorcount = 0;

my $URLScount = 0;

for my $u (@urls) {
  if (defined($ignoredURLS{$u})) {
    $ignoredURLS{$u}->{count} += 1;
    next;
  }
  my $use_curl = 0;
  if (defined($knownToRegisterURLS{$u})) {
    if ($knownToRegisterURLS{$u}->{use_curl}) {
      $use_curl = 1;
    }
    else {
      next;
    }
  }
  if (defined($selectedURLS{$u})) {
    ${selectedURLS}{$u}->{count} += 1;
  }
  next if ($checkSelectedOnly && !defined($selectedURLS{$u}));
  $URLScount++;
  push(@testvals, {u => $u, use_curl => $use_curl,});
  my $uorig = $u;
  $u = constructExtraTestUrl($uorig);
  if ($u ne $uorig) {
    if (!defined($selectedURLS{$u})) {
      if (!defined($extraTestURLS{$u})) {
        $extraTestURLS{$u} = 1; # omit multiple tests
        push(@testvals, {u => $u, use_curl => $use_curl, extra => 1});
        $URLScount++;
      }
    }
  }
}

# Ready to go multitasking
my ($vol, $dir, $file) = File::Spec->splitpath($summaryFile);
my $tempdir   = tempdir("$dir/CounterXXXXXXX", CLEANUP => 1);
my $countfile = "$tempdir/counter";
my $counter   = 0;
if (open(my $FO, '>', $countfile)) {
  print {$FO} $counter;
  close($FO);
}
else {
  unlink($countfile);
  die("Could not write to $countfile");
}

print "Using tempdir \"" . abs_path($tempdir) . "\"\n";

my %wait = ();
for (my $i = 0; $i < $NR_JOBS; $i++) {    # Number of subprocesses
  my $pid = fork();
  if ($pid > 0) {
    $wait{$pid} = $i;
  }
  elsif ($pid == 0) {

    # I am child
    open(my $fe, '>:encoding(UTF-8)', "$tempdir/xxxError$i");
    my $subprocess = $i;
    open(my $fs, '>:encoding(UTF-8)', "$tempdir/xxxSum$i");
    while (1) {
      open(my $fh, '+<', $countfile) or die("cannot open $countfile");
      flock($fh, LOCK_EX)            or die "$i: Cannot lock $countfile - $!\n";
      my $l = <$fh>;    # get actual count number
      if (!defined($testvals[$l])) {
        close($fs);
        print $fe "NumberOfErrors $errorcount\n";
        close($fe);
        exit(0);
      }
      my $diff = getnrjobs(scalar @testvals, $l, $NR_JOBS);
      if ($diff < 1) {
        $diff = 1;
      }
      my $next = $l + $diff;
      seek($fh, 0, 0);
      truncate($fh, 0);
      print $fh $next;
      close($fh);
      for (my $i = 0; $i < $diff; $i++) {
        my $entryidx = $l + $i;
        my $rentry   = $testvals[$entryidx];
        next if (!defined($rentry));
        my $u        = $rentry->{u};
        my $use_curl = $rentry->{use_curl};
        my $extra    = defined($rentry->{extra});

        print $fe "Checking($entryidx-$subprocess) '$u': time=" . time() . ' ';
        my ($res, $prnt, $outSum);
        try {
          $res = check_url($u, $use_curl, $fe, $fs);
          if ($res) {
            print $fe "Failed\n";
            $prnt   = "";
            $outSum = 1;
          }
          else {
            $prnt   = "OK\n";
            $outSum = 0;
          }
        }
        catch {
          $prnt   = "Failed, caught error: $_\n";
          $outSum = 1;
          $res    = 700;
        };
        printx("$prnt", $outSum, $fe, $fs);
        my $printSourceFiles = 0;
        my $err_txt;
        if ($extra) {
          $err_txt = "Extra_Error url:";
        }
        else {
          $err_txt = "Error url:";
        }

        if ($res || $checkSelectedOnly) {
          $printSourceFiles = 1;
        }
        if ($res && defined($revertedURLS{$u})) {
          $err_txt = "Failed url:";
        }
        $res = !$res if (defined($revertedURLS{$u}));
        if ($res || $checkSelectedOnly) {
          printx("$err_txt \"$u\"\n", $outSum, $fe, $fs);
        }
        else {
          my $succes;
          if ($extra) {
            # This url is created
            $succes = "Extra_OK url:";
          }
          else {
            $succes = "OK url:";
          }
          printx("$succes \"$u\"\n", $outSum, $fe, $fs);
          $printSourceFiles = 1;
        }
        if ($printSourceFiles) {
          if (defined($URLS{$u})) {
            for my $f (sort keys %{$URLS{$u}}) {
              my $lines = ":" . join(',', @{$URLS{$u}->{$f}});
              printx("  $f$lines\n", $outSum, $fe, $fs);
            }
          }
          if ($res) {
            $errorcount++;
          }
        }
      }
    }
  }
}

sub readsublog($) {
  my ($i) = @_;
  open(my $fe, '<:encoding(UTF-8)', "$tempdir/xxxError$i");
  while (my $l = <$fe>) {
    if ($l =~ /^NumberOfErrors\s(\d+)/) {
      $errorcount += $1;
    }
    else {
      print $l;
    }
  }
  close($fe);
  open(my $fs, '<', "$tempdir/xxxSum$i");
  while (my $l = <$fs>) {
    print SFO $l;
  }
  close($fs);
}

my $p;
do {
  $p = waitpid(-1, 0);
  if (($p > 0) && defined($wait{$p}) && $wait{$p} >= 0) {
    &readsublog($wait{$p});
    $wait{$p} = -1;
  }
} until ($p < 0);
print "Started to protocol remaining subprocess-logs\n";

for my $p (keys %wait) {
  if ($wait{$p} >= 0) {
    &readsublog($wait{$p});
    $wait{$p} = -1;
  }
}
print "Stopped to protocol remaining subprocess-logs\n";
unlink($countfile);

if (%URLS) {
  printNotUsedURLS("Ignored",      %ignoredURLS);
  printNotUsedURLS("Selected",     %selectedURLS);
  printNotUsedURLS("KnownInvalid", %extraURLS);
}

print SFO "\n$errorcount URL-tests failed out of $URLScount\n\n";
if (defined($summaryFile)) {
  close(SFO);
}
exit($errorcount);

###############################################################################
sub printx($$$$) {
  my ($txt, $outSum, $fe, $fs) = @_;
  print $fe "$txt";
  if ($outSum && defined($summaryFile)) {
    print $fs "$txt";
  }
}

sub printNotUsedURLS($\%) {
  my ($txt, $rURLS) = @_;
  my @msg = ();
  for my $u (sort keys %{$rURLS}) {
    if ($rURLS->{$u}->{count} < 2) {
      my @submsg = ();
      for my $f (sort keys %{$rURLS->{$u}}) {
        next if ($f eq "count");
        push(@submsg, "$f:" . $rURLS->{$u}->{$f});
      }
      push(@msg, "\n  $u\n    " . join("\n    ", @submsg) . "\n");
    }
  }
  if (@msg) {
    print "\n$txt URLs: " . join(' ', @msg) . "\n";
  }
}

sub replaceSpecialChar($) {
  my ($l) = @_;
  $l =~ s/\\SpecialChar(NoPassThru)?\s*(TeX|LaTeX|LyX)[\s]?/$2/;
  $l =~ s/ /%20/g;
  return ($l);
}

sub readUrls($\%) {
  my ($file, $rUrls) = @_;

  die("Could not read file $file") if (!open(ULIST, '<:encoding(UTF-8)', $file));
  print "Read urls from $file\n";
  my $line = 0;
  while (my $l = <ULIST>) {
    $line++;
    chomp($l);                  # remove eol
    $l =~ s/^\s+//;
    next if ($l =~ /^\#/);      # discard comment lines
    next if ($l eq "");
    $l = &replaceSpecialChar($l);
    my $use_curl = 0;
    if ($l =~ s/^UseCurl\s*//) {
      $use_curl = 1;
    }
    if (!defined($rUrls->{$l})) {
      $rUrls->{$l} = {$file => $line, count => 1, use_curl => $use_curl};
    }
  }
  close(ULIST);
}

sub parse_file($) {
  my ($f) = @_;
  my $status = "out";    # outside of URL/href

  #return if ($f =~ /\/attic\//);
  if (open(FI, '<:encoding(UTF-8)', $f)) {
    my $line = 0;
    while (my $l = <FI>) {
      $line++;
      chomp($l);

      if ($status eq "out") {

        # searching for "\begin_inset Flex URL"
        if ($l =~ /^\s*\\begin_inset\s+Flex\s+URL\s*$/) {
          $status = "inUrlInset";
        }
        elsif ($l =~ /^\s*\\begin_inset\s+CommandInset\s+href\s*$/) {
          $status = "inHrefInset";
        }
        else {
          # Outside of url, check also
          if ($l =~ /"((ftp|http|https):\/\/[^ ]+)"/) {
            my $url = $1;
            handle_url($url, $f, "x$line");
          }
        }
      }
      else {
        if ($l =~ /^\s*\\end_(layout|inset)\s*$/) {
          $status = "out";
        }
        elsif ($status eq "inUrlInset") {
          if ($l =~ /\s*([a-z]+:\/\/.+)\s*$/) {
            my $url = $1;
            $status = "out";
            handle_url($url, $f, "u$line");
          }
        }
        elsif ($status eq "inHrefInset") {
          if ($l =~ /^target\s+"([a-z]+:\/\/[^ ]+)"$/) {
            my $url = $1;
            $status = "out";
            handle_url($url, $f, "h$line");
          }
        }
      }
    }
    close(FI);
  }
}

sub handle_url($$$) {
  my ($url, $f, $line) = @_;

  $url = &replaceSpecialChar($url);
  if (!defined($URLS{$url})) {
    $URLS{$url} = {};
  }
  if (!defined($URLS{$url}->{$f})) {
    $URLS{$url}->{$f} = [];
  }
  push(@{$URLS{$url}->{$f}}, $line);
}

sub getnrjobs($$$) {
  my ($tabsize, $actualidx, $nr_jobs) = @_;
  my $maxidx    = $tabsize - 1;
  my $remaining = $maxidx - $actualidx;
  if ($remaining <= 0) {
    return (1);
  }
  if ($nr_jobs < 2) {
    return ($remaining);
  }
  my $diff = 1 + int($remaining / (3 * $nr_jobs));
  return $diff;
}
