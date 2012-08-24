#! /usr/bin/env perl

use strict;
use warnings;
no warnings 'uninitialized';

my %opcodes;

sub unhex {
  return hex $_[0];
}

sub instr_jumptable {
  for(my $i=0;$i<256;$i++) {
    print sprintf("J(0x%2.2X), ",$i);
  }
}

sub instr_opcode {
  local $_ = $_[0];
  s/ +.*$//;
  print sprintf("OPCODE_BYTE0(0x%2.2X):",hex $_);
  $opcodes{hex $_} = undef;
}

sub instr_opcoderest {
  for(my $i=0;$i<256;$i++) {
    next if exists $opcodes{$i};
    print sprintf("OPCODE_BYTE0(0x%2.2X): INVALID_OPCODE(0x%2.2X);\n",$i,$i);
  }
  
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

