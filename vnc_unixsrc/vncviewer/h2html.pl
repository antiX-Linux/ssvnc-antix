#!/usr/bin/perl

open(HELP, "./vncviewer -help|");

while (<HELP>) {
	$_ =~ s/&/&amp;/g;
	$_ =~ s/</&lt;/g;
	$_ =~ s/>/&gt;/g;
	print;
}
