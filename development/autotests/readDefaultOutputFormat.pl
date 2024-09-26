#! /usr/bin/env perl
# -*- mode: perl; -*-

#
use strict;

my $useNonTexFonts = undef;
my $outputFormat = undef;
my $texFormat = undef;
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
    if (defined($ARGV[1])) {
      print "outputformat = \"$outputFormat\"\n";
      print "useNonTexFonts = \"$useNonTexFonts\"\n";
      print "language = \"$language\"\n";
    }
  }
}
if ($language eq "japanese") {
  if ($useNonTexFonts) {
    if ($outputFormat =~ /^(default|pdf4)$/) {
      $outputFormat = "pdf4";
      $texFormat = "xetex";
    }
    elsif ($outputFormat =~ /^pdf[35]?$/) {
      if ($outputFormat =~ /^pdf3?$/) {
        $texFormat = "platex";
      }
      else {
        $texFormat = "luatex";
      }
    }
    else {
      $outputFormat = undef;
    }
  }
  else { # using tex font
    if ($outputFormat =~ /^(default|pdf3)$/) {
      $outputFormat = "pdf3";
      $texFormat = "platex";
    }
    elsif ($outputFormat =~ /^pdf5$/) {
      $texFormat = "luatex";
    }
    else {
      $outputFormat = undef;
    }
  }
}
else { # not a japanese language
  if ($useNonTexFonts) {
    if ($outputFormat =~ /^(default|pdf4)$/) {
      $outputFormat = "pdf4";
      $texFormat = "xetex";
    }
    elsif ($outputFormat eq "pdf5") {
      $texFormat = "luatex";
    }
    else {
      $outputFormat = undef;
    }
  }
  else { # using tex fonts
    if ($outputFormat =~ /^(default|pdf2)$/) {
      $outputFormat = "pdf2";
      $texFormat = "pdflatex";
    }
    elsif ($outputFormat eq "pdf5") {
      $texFormat = "luatex";
    }
    elsif ($outputFormat eq "pdf3") {
      $texFormat = "latex";
    }
    else {
      $outputFormat = undef;
    }
  }
}

if (defined($outputFormat)) {
  print "$texFormat/$outputFormat";
}
else {
  print "undefined_output_format";
}

exit(0);
