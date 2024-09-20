#! /usr/bin/env perl
# -*- mode: perl; -*-

#
use strict;

my $useNonTexFonts = undef;
my $outputFormat = undef;
my $outputFormat = undef;
my $language = undef;
if (-e "$ARGV[0]") {
  if (open(FI, "$ARGV[0]")) {
    while (my $l = <FI>) {
      chomp($l);
      if ($l =~ /^\\use_non_tex_fonts\s+([a-z]+)/) {
        $useNonTexFonts = ($1 eq "true");
      }
      elsif ($l =~ /^\\default_output_format\s+([a-z0-9]+)/) {
        $outputFormat = $1;
      }
      elsif ($l =~ /\\language\s+([\-a-z_]+)/) {
        $language = $1;
      }
      last if (defined($useNonTexFonts) && defined($outputFormat) && defined($language));
    }
    close(FI);
  }
}
if (defined($useNonTexFonts) && defined($outputFormat)) {
  if ($useNonTexFonts) {
    if ($outputFormat eq "default") {
      if ($language eq "japanese") {
        $outputFormat = "pdf4";
      }
      else {
        $outputFormat = "pdf5";
      }
    }
  }
  elsif ($outputFormat eq "default") {
    if ($language eq "japanese") {
      $outputFormat = "pdf3";
    }
    else {
      $outputFormat = "pdf2";
    }
  }
  if ($outputFormat !~ /^pdf/) {
    $outputFormat = undef;
  }
}

if (defined($outputFormat)) {
  print "$outputFormat";
}
else {
  print "undefined_output_format";
}

exit(0);
