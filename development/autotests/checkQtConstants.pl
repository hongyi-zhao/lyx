#! /usr/bin/env perl
# -*- mode: perl; -*-
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

for my $ui (@UIFiles) {
  checkConstants($ui);
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
      if ($l =~ /\bQ([a-zA-Z]+\:\:){3}/) {
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
