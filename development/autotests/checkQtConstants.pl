#! /usr/bin/env perl
# -*- mode: perl; -*-
#
# Checking for allowed Qt constants in src/frontends/qt/ui/*.ui
# Allowed form: Q[A_Za-z]+::[A-Za-z]+
# See https://doc.qt.io/qt-6/qt.html
# Therefore, forms like Q[A_Za-z]+(::[A-Za-z]+){2,} are considered not allowed.
#
use strict;
use warnings;

BEGIN {
  use File::Spec;
  my $p = File::Spec->rel2abs(__FILE__);
  $p =~ s/[\/\\]?[^\/\\]+$//;
  unshift(@INC, "$p");
}

my $no_founds = 0;
sub checkConstants($);
sub collectUiFiles($$);

my @UIFiles = ();

collectUiFiles('.', \@UIFiles);
print "Checking Qt constants according to https://doc.qt.io/qt-6/qt.html\n";
for my $ui (@UIFiles) {
  checkConstants($ui);
}
if ($no_founds == 0) {
  print "...\nNo offending values found\n";
}
else {
  print "$no_founds places with questionable names for qt-constants found\n";
}
exit($no_founds);

###########################################

sub checkConstants($) {
  my ($ui) = @_;
  #print "Checking $ui\n";
  if (open(my $FI, '<', $ui)) {
    my $lineno = 0;
    while (my $l = <$FI>) {
      $lineno += 1;
      if ($l =~ /\bQ([a-zA-Z]+\:\:){2}/) {
        print "$ui:$lineno $l";
        $no_founds += 1;
      }
    }
    close($FI);
  }
}

# Recursive collect UI files
sub collectUiFiles($$) {
  my ($dir, $rFiles) = @_;
  my @subdirs = ();
  if (opendir(my $DI, $dir)) {
    while (my $f = readdir($DI)) {
      if (-d "$dir/$f") {
        next if ($f =~ /^\.(\.)?$/);
        push(@subdirs, "$dir/$f");
      }
      elsif ($f =~ /\.ui$/) {
        push(@{$rFiles}, "$dir/$f");
      }
    }
    closedir($DI);
  }
  for my $d (@subdirs) {
    collectUiFiles($d, $rFiles);
  }
}
