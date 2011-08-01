#!/usr/bin/perl

# disable I/O buffering (this is essential)
$| = 1;

while(<>)
{
	# 2 colors
	print "2,4,red,yellow,bold\n";
	print "6,7,green,white\n";
	print "\n";
}
