require('cups');

var b64 = File.read('test.b64');

var ret = cups.print('CT-S310', b64, 203, 719);
puts(ret);

