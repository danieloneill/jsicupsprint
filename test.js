#!/usr/bin/env jsish

require('cups');

var list = cups.printerlist();
puts( list );

puts( cups.printerinfo( list[0]['printer-info'], list[0]['device-uri'] ) );

delete list;

/*
var b64 = File.read('test.b64');

var ret = cups.print('CT-S310', b64, 203, 719);
puts(ret);
*/
