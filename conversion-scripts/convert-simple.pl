#!/usr/bin/perl

# disable I/O buffering (this is essential)
$| = 1;

while(<>)
{
	# add 'bla' in front of the string
	print "bla".$_;
}
