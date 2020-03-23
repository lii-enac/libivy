#!/usr/bin/tclsh
load ../src/libtclivy.so.3.6

proc connect { client action } { }

proc send { } {
  global tosend
  Ivy::send $tosend
}

proc dotext {client} {
  global id
  puts "$client told me to unbind (to $id)"
  Ivy::unbind $id
}

Ivy::init "TCLTK" "TCLTK Ready" connect connect
Ivy::start ""
set id [Ivy::bind "^unbind" dotext]
puts "bound to value $id"

vwait tcl_interactive
