# jsicupsprint
A very simple JSI module to print an image/PS/PDF in Base64 encoding via CUPS

You'll need the CUPS headers and libraries and junk, GraphicsMagick libraries and headers and whatever, and JSI.

If you don't know what CUPS or JSI are, this isn't for you.

You can edit the Makefile to point it to your jsish location.

Once built, copy the **cups.so** file to **/usr/local/lib/jsi/cups.so**

You may need to create that directory.

Using it is as simple as pie:

```
require('cups');
var b64content = File.read('test.b64'); // <- this is base64 encoded. you can use a PNG, JPG, PDF, whatever GraphicsMagick can read.
cups.print('My Printer', b64content, 203, 719);
```

The parameters are: __[printer name], [base64 encoded content], [resolution in dpi], [page width in mm]__

I know.. I know... dots per inch, width in mm, but that's how the Citizen CT-S310 I use is specced, so that's how I wrote it.

This doesn't ask for page height because it's made to print to a thermal printer. You know, a receipt printer. For a store, see?

Don't like it?! Well... hack the code. It's super simple. The glue is in *cups.jsc* and the meat is in *cupsinc.c*.

Dogspeed.
