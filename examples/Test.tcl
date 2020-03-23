#!/usr/bin/tclsh

load ../src/libtclivy.so.3.6

proc connxCB { name event } {
 puts "an Ivy agent named $name $event"
}

proc msgCB {client str} {
  puts "received $str from client $client"
}

Ivy::init "TESTTCL" "TESTTCL Ready" connxCB connxCB 
set id [ Ivy::bind "(.*)" msgCB  ]
Ivy::start ""

vwait tcl_interactive
