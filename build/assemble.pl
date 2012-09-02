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
  next if(/^\s*$/ or /^\s*;/);
  my @line = split /\s+/;
  $opcodes{$line[0]} = {
    code => dehex($line[1]),
    type => $line[2],
    extra => $line[3],
    offset => $line[4],
  };
}
close DATAFILE;

my %instr;

open(DATAFILE,"vm/instr.dat") || die "Cannot read instr.dat";
while(<DATAFILE>) {
  next if(/^\s*$/ or /^\s*;/);
  /^(.*?)\s+\-\>\s+(.*?)$/;
  $instr{$1}=$2;
}
close DATAFILE;

sub word {
  my ($r,$a,$b,$c) = @_;
  
  return ($r<<24)|($a<<16)|($b<<8)|$c;
}

my (@output,@offset,%labels);
my $offset = 0;

sub convert {
  local $_ = shift;
  my $instr = shift;
  my $mode = shift;
  
  my $orig = $_;
  my $neg = s/^-//;
  my $val = 0;
  if(s/^0x//) {
    $val = hex $_;
  } elsif(/^[0-9]+/) {
    $val = $_+0;
  } else {
    unless(defined $labels{$_}) {
      if($mode) {
        die "Bad label '$orig'\n";
      } else {
        $val = 0;
      }
    }
    my $jump = $labels{$_}-$offset;
    if($instr->{'offset'} =~ /F(-?\d+)/) {
      $jump += $1;
    }
    $val = $jump;
    if($val<0) { $val*=-1; $neg=1; }
  }
  if($neg) {
    $val--;
    $val ^= 0xFFFFFFFF;
  }
  return $val;
}

sub trim {
  local $_ = shift;
  s/^\s+//;
  s/\s+$//;
  return $_;
}

sub type_reg {
  local $_ = shift;
  return $1 if /^r(\d+)$/;
  return undef;
}

sub type_label {
  local $_ = shift;
  return $_ if /^[A-Za-z0-9_]+$/;
  return undef;
}

sub parse {
  my ($mode) = @_;

  my $label = '';
  $offset = 0;
  open(INFILE,$filename) || die "Cannot read '$filename'\n";
  while(<INFILE>) {
    my @outline;
    next if(/^\s*$/ or /^\s*;/);
    $_ = trim $_;
    s/;.*$//;
    if(/^\.(.*)/) {
      $labels{$1} = $offset unless $mode;
      $label .= "$_ ";
      next;
    }
    my $line = $_;
    my $orig_line = $line;
    $line =~ /^\s*(\S+)(\s+(.*?))?\s*$/;
    my ($ins,$rest) = ($1,$2);
    my @repl;
    foreach my $tmpl (keys %instr) {
      my @cand;
      my @tmpl = split /\s+/,trim($tmpl);
      my $bins = $ins;
      my $ad = 0;
      # Handle AD's in &'s
      my $ti = shift @tmpl;
      if($ti =~ /^(.*?)\&(.*?)$/) {
        my ($a,$b) = ($1,$2);
        next unless($ins =~ /$a([adAD]*)$b/);
        $bins = "$a\&$b";
        my @ad = split(//,$1);
        while((my $v = lc shift @ad)) {
          $ad = ($ad*4)+(($v eq 'a')?2:1);
        }
      }
      #
      next unless $ti eq $bins;
      push @cand,$bins;
      my @args = split /,/,$rest;
      foreach my $tmpl_arg (@tmpl) {
        my $real_arg = trim(shift @args);
        if($tmpl_arg =~ /\[(.*?)\]/) {
          no strict;
          push @cand,&{"type_$1"}($real_arg);
        } else {
          my $v;
          $v = $real_arg if($real_arg eq $tmpl_arg);
          push @cand,$v;
        }
      }
      my $repl = $instr{$tmpl};
      foreach my $i (0..$#cand) {
        $repl =~ s/\$$i/$cand[$i]/ge;
      }
      $repl =~ s/\&/$ad/ge;
      next if grep { !defined $_ } @cand;
      $line = trim $repl;
      #print STDERR "LINE '$line'\n";
      last;
    }
    my @line = split /\s+/,$line;
    my $instr = $opcodes{shift @line};
    die "Unknown opcode '$line[0]' in $line" unless defined $instr;
    @line = map { convert($_,$instr,$mode) } @line;
    my @args;
    my $args = 3-(ord($instr->{'type'})-ord('A'));
    push @args,(shift @line) for (0..$args-1);
    unshift @args,0 for(1..(3-@args));
    push @outline,word($instr->{'code'},@args);
    if($instr->{'extra'} =~ /V/) {
      push @outline,(shift @line);
    }
    if($mode) {
      push @output,[\@outline,"$label${orig_line} [$line]"];
    } else {
      push @offset,length(@outline);
    }
    $offset += @outline;
    $label = '';
  }
  close INFILE;
}

parse(0);
parse(1);

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
