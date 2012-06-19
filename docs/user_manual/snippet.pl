#!/usr/bin/perl -w
# 
# Copyright (C) 2009-2011 Chris McClelland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#  
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#print $#ARGV;

if ( $#ARGV != 1 ) {
	print STDERR "Synopsis: $0 <file> <snippet>\n";
	exit(1);
}

my $file = $ARGV[0];
my $snippet = $ARGV[1];

undef $/;
open FILE, $file or die "Cannot open file";
my $contents = <FILE>;
close FILE;
my $firstMatch = 1;
my $returnCode = 0;

while ( $contents =~ m/BEGIN_SNIPPET\($snippet\)\n(.*?)\s+..END_SNIPPET\($snippet\)/mgs ) {
	 my $chunk = $1;
	 my @lines = split(/\n/, $chunk);
	 my $minIndent = 1000;
	 my $numTabs;
	 my $line;
	 foreach ( @lines ) {
		  $line = $_;
		  if ( !($line =~ m/^(\s*)$/) ) {
				$line =~ m/^(\t*)/;
				$numTabs = length($1);
				if ( $numTabs < $minIndent ) {
					 $minIndent = $numTabs;
				}
		  }
	 }
	 if ( $firstMatch ) {
		  $firstMatch = 0;
	 } else {
		  print "   :\n";
	 }
	 foreach ( @lines ) {
		  $line = $_;
		  if ( $line =~ m/^(\s*)$/ ) {
				print "\n";
		  } else {
				$line = substr($line, $minIndent);
				$line =~ s/\t/   /mgs;
				if ( length($line) > 80 ) {
					 print STDERR "Line too long: $line\n";
					 $returnCode = 1;
				}
				if ( $line =~ m/^(.*?)\s+..(BEGIN|END)_SNIPPET\(/ ) {
					 $line = $1;
					 print "$line\n";
				} else {
					 print "$line\n";
				}
		  }
	 }
}

if ( $returnCode ) {
	 exit($returnCode);
}
