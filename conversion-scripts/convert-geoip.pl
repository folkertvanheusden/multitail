#!/usr/bin/perl

use Geo::IP;

my $gi = Geo::IP->new(GEOIP_STANDARD);

$| = 1;

while(<>)
{
	chomp($_);

	$country = $gi->country_code_by_addr($_);

	if ($country eq '')
	{
		$country = '?';
	}

	print "$_ ($country)\n";
}
