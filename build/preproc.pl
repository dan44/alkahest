#! /usr/bin/env perl

use strict;
use warnings;
no warnings 'uninitialized';

use FindBin qw($Bin);

my (%code2symbol,%symbol2code);

sub instr_jumptable {
  for(my $i=0;$i<256;$i++) {
    if($code2symbol{$i}) {
      print sprintf("J(%s), ",$code2symbol{$i});
    } else {
      print "J(invalid), ";
    }
  }
}

sub instr_opcode {
  local $_ = $_[0];
  s/ +.*$//;
  print sprintf("OPCODE_BYTE0(%s):",$_);
}

sub instr_defcodes {
  my @args = split(/\s+/,shift);
  open(CODES,$args[0]) || die "Cannot read '$args[0]'\n";
  while(<CODES>) {
    next if(/^\s*$/ or /^\s*;/);
    my @args = split(/\s+/,$_);
    my $hex = $args[1];
    $hex =~ s/^0x//;
    print sprintf("#define O_%-20s $args[1]\n",$args[0]);
    my @dl = split(//,"abc");
    pop @dl for (1..(ord($args[2])-ord('A')));
    my @ul = @dl;
    unshift @ul,"O_$args[0]";
    if($args[3] eq 'V') {
      push @dl,"v";
    }
    my $dl="";
    $dl="(".join(",",@dl).")" if @dl;
    print "#define I_$args[0]$dl I_$args[2](".join(",",@ul).")";
    if($args[3] eq 'V') {
      print ", (v)";
    }
    print "\n";
  }
  close CODES;
}

sub instr_readcodes {
  my @args = split(/\s+/,shift);
  open(CODES,$args[0]) || die "Cannot read '$args[0]'\n";
  while(<CODES>) {
    next if(/^\s*$/ or /^\s*;/);
    my @args = split(/\s+/,$_);
    my $hex = $args[1];
    $hex =~ s/^0x//;
    $symbol2code{$args[0]} = hex $hex;
    $code2symbol{hex $hex} = $args[0];
  }
  close CODES;
}

while(<STDIN>) {
  if(/^##(\S*)\s+(.*)$/) {
    no strict;
    &{"instr_$1"}($2);
  } else {
    print $_;
  }
}

1;

