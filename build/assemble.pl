#! /usr/bin/env perl

use strict;
use warnings;
no warnings 'uninitialized';

my ($filename) = @ARGV;

my %opcodes;

sub dehex {
  local $_ = shift;
  s/^\s*0x//;
  return hex $_;
}

open(DATAFILE,"vm/opcodes.dat") || die "Cannot read opcodes.dat";
while(<DATAFILE>) {
  next if(/^\s*$/ or /^\s*#/);
  my @line = split /\s+/;
  $opcodes{$line[0]} = {
    code => dehex($line[1]),
    type => $line[2],
    extra => $line[3],
  };
}
close DATAFILE;

sub word {
  my ($r,$a,$b,$c) = @_;
  
  return ($r<<24)|($a<<16)|($b<<8)|$c;
}

sub convert {
  local $_ = shift;
  
  my $orig = $_;
  my $neg = s/^-//;
  my $val = 0;
  if(s/^0x//) {
    $val = hex $_;
  } elsif(/^[0-9]+/) {
    $val = $_+0;
  } else {
    die "Bad argument '$orig'\n";
  }
  if($neg) {
    $val--;
    $val ^= 0xFFFFFFFF;
  }
  return $val;
}

my @output;

open(INFILE,$filename) || die "Cannot read '$filename'\n";
while(<INFILE>) {
  my @outline;
  next if(/^\s*$/ or /^\s*#/);
  s/^\s+//;
  s/\s+$//;
  my $line = $_;
  my @line = split /\s+/;
  my $instr = $opcodes{shift @line};
  die "Unknown opcode $line[0]" unless defined $instr;
  @line = map { convert $_ } @line;
  my @args;
  my $args = 3-(ord($instr->{'type'})-ord('A'));
  push @args,(shift @line) for (0..$args-1);
  unshift @args,0 for(1..(3-@args));
  push @outline,word($instr->{'code'},@args);
  if($instr->{'extra'} =~ /V/) {
    push @outline,(shift @line);
  }
  push @output,[\@outline,$line];
}
close INFILE;

my $outfile = $filename;
$outfile =~ s/.aa$//;
$outfile =~ s/^.*?\///;

my @lines;
foreach my $line (@output) {
  my $out = "  ";
  my ($hex,$line) = @$line;
  foreach (@$hex) {
    $out .= sprintf("0x%8.8X, ",$_);
  }
  $out .= sprintf("/* %s */",$line);
  push @lines,$out;
}

print sprintf(<<"EOF",join("\n",@lines));
#ifndef ${outfile}_aa_H
#define ${outfile}_aa_H

uint32_t aa_${outfile}[] = {
%s
};

#endif
EOF

1;
