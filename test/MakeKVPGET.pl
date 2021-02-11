#! /usr/bin/perl

use strict;

MakeXMLPost($ARGV[0], $ARGV[1], "/wfs");

sub MakeXMLPost
{
    my ($input_fn, $output_fn, $url_path) = @_;
    my $data = ReadFile($input_fn);
    my $len = length($data);
    my $output;
    open $output, ">$output_fn" or die "Failed to open output file $output_fn: $!";
    # print "Writing $output_fn\n";
    print $output "GET $url_path?$data HTTP/1.1\r\n";
    print $output "Host: brainstormgw.fmi.fi\r\n";
    print $output "fmi-apikey: foobar\r\n";
    print $output  "\r\n";
    print $output  "\r\n";
}

sub ReadFile
{
    my $fn = $_[0];
    my ($fd, $data);
    open $fd, $fn or die "Failed to open $fn: $!";
    $data = <$fd>;
    chomp($data);
    return $data;
}
