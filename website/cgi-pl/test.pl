#!/usr/bin/perl
use strict;
use warnings;

sub escape_html {
    my ($value) = @_;
    $value = '' unless defined $value;
    $value =~ s/&/&amp;/g;
    $value =~ s/</&lt;/g;
    $value =~ s/>/&gt;/g;
    $value =~ s/"/&quot;/g;
    $value =~ s/'/&#39;/g;
    return $value;
}

sub url_decode {
    my ($value) = @_;
    $value =~ tr/+/ /;
    $value =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/ge;
    return $value;
}

sub parse_query {
    my ($raw) = @_;
    my @pairs;
    for my $pair (split /[&;]/, $raw) {
        next if $pair eq '';
        my ($key, $value) = split /=/, $pair, 2;
        push @pairs, [url_decode($key), url_decode(defined $value ? $value : '')];
    }
    return @pairs;
}

sub print_params {
    my ($title, @pairs) = @_;
    return unless @pairs;
    print "<h3>$title</h3>\n<ul>\n";
    for my $pair (@pairs) {
        print "<li><strong>" . escape_html($pair->[0]) . ":</strong> "
            . escape_html($pair->[1]) . "</li>\n";
    }
    print "</ul>\n";
}

print "Content-Type: text/html; charset=UTF-8\r\n\r\n";
print <<'HTML';
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Perl CGI Test</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<div class="container">
<h1>Perl CGI Working!</h1>
<div class="section">
<h2>Request Info</h2>
HTML

print "<p><strong>Perl Version:</strong> " . escape_html(sprintf('%vd', $^V)) . "</p>\n";
for my $item (
    ['Method', 'REQUEST_METHOD'],
    ['Query String', 'QUERY_STRING'],
    ['Script Name', 'SCRIPT_NAME'],
    ['Path Info', 'PATH_INFO'],
    ['Path Translated', 'PATH_TRANSLATED'],
    ['Server Protocol', 'SERVER_PROTOCOL'],
    ['Server Name', 'SERVER_NAME'],
    ['Server Port', 'SERVER_PORT'],
    ['Gateway Interface', 'GATEWAY_INTERFACE'],
) {
    print "<p><strong>$item->[0]:</strong> " . escape_html($ENV{$item->[1]}) . "</p>\n";
}

print_params('GET Parameters:', parse_query(defined $ENV{QUERY_STRING} ? $ENV{QUERY_STRING} : ''));

my $content_length = defined $ENV{CONTENT_LENGTH} ? $ENV{CONTENT_LENGTH} : '';
if ($content_length =~ /^\d+$/ && $content_length > 0) {
    binmode STDIN;
    my $body = '';
    while (length($body) < $content_length) {
        my $read = read(STDIN, $body, $content_length - length($body), length($body));
        last unless $read;
    }
    print_params('POST Parameters:', parse_query($body));
}

print "<h3>Environment Variables (HTTP_*):</h3>\n<ul>\n";
for my $name (sort grep { /^HTTP_/ } keys %ENV) {
    print "<li><strong>" . escape_html($name) . ":</strong> "
        . escape_html($ENV{$name}) . "</li>\n";
}
print "</ul>\n</div>\n";

print <<'HTML';
<div class="section">
<h2>Test Forms</h2>
<h3>GET Test</h3>
<form action="/cgi-pl/test.pl" method="GET">
<input type="text" name="test_get" placeholder="GET parameter">
<button type="submit">Send GET</button>
</form>
<h3>POST Test</h3>
<form action="/cgi-pl/test.pl" method="POST">
<input type="text" name="test_post" placeholder="POST parameter">
<button type="submit">Send POST</button>
</form>
</div>
<p><a href="/">Back to home</a></p>
</div>
</body>
</html>
HTML
