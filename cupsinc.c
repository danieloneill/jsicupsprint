#include <cups/cups.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include <png.h>

// Base64 shit:
static const unsigned char base64_table[65] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
unsigned char * base64_decode(const unsigned char *src, size_t len,
                              size_t *out_len)
{
        unsigned char dtable[256], *out, *pos, block[4], tmp;
        size_t i, count, olen;
        int pad = 0;

        memset(dtable, 0x80, 256);
        for (i = 0; i < sizeof(base64_table) - 1; i++)
                dtable[base64_table[i]] = (unsigned char) i;
        dtable['='] = 0;

        count = 0;
        for (i = 0; i < len; i++) {
                if (dtable[src[i]] != 0x80)
                        count++;
        }

        if (count == 0 || count % 4)
                return NULL;

        olen = count / 4 * 3;
        pos = out = malloc(olen);
        if (out == NULL)
                return NULL;

        count = 0;
        for (i = 0; i < len; i++) {
                tmp = dtable[src[i]];
                if (tmp == 0x80)
                        continue;

                if (src[i] == '=')
                        pad++;
                block[count] = tmp;
                count++;
                if (count == 4) {
                        *pos++ = (block[0] << 2) | (block[1] >> 4);
                        *pos++ = (block[1] << 4) | (block[2] >> 2);
                        *pos++ = (block[2] << 6) | block[3];
                        count = 0;
                        if (pad) {
                                if (pad == 1)
                                        pos--;
                                else if (pad == 2)
                                        pos -= 2;
                                else {
                                        /* Invalid padding */
                                        free(out);
                                        return NULL;
                                }
                                break;
                        }
                }
        }

        *out_len = pos - out;
        return out;
}
// PNG 2 JPEG shit:
#include <magick/api.h>
int image2pdf( int width, double dpi, const char *indata, size_t inlen, char **outdata, size_t *outlen )
{
    ExceptionInfo exception;

    InitializeMagick(NULL);
    ImageInfo *info = CloneImageInfo(0);
    GetExceptionInfo(&exception);

    Image *img = BlobToImage( info, indata, inlen, &exception );

    long cols = (double)(dpi * width); // width in inches.
    long rows = (double)((double)cols / (double)img->columns) * img->rows;
    //printf("Resizing from %ldx%ld to %ldx%ld\n", img->columns, img->rows, cols, rows);
    Image *nimg = ScaleImage(img, cols, rows, &exception);
    DestroyImage(img);

    //OrderedDitherImage( nimg );
    strcpy(nimg->magick, "PDF" );
    info->monochrome = 1;
    info->dither = 1;
    nimg->units = PixelsPerInchResolution;
    nimg->x_resolution = dpi;
    nimg->y_resolution = dpi;

    *outdata = ImageToBlob( info, nimg, outlen, &exception );
    DestroyImage(nimg);
    DestroyImageInfo(info);

    DestroyExceptionInfo(&exception);
    DestroyMagick();

    return 0;
}

// CUPS shit:
typedef struct
{
  int num_dests;
  cups_dest_t *dests;
} my_user_data_t;

int
my_dest_cb(my_user_data_t *user_data, unsigned flags,
           cups_dest_t *dest)
{
  if (flags & CUPS_DEST_FLAGS_REMOVED)
  {
   /*
    * Remove destination from array...
    */

    user_data->num_dests =
        cupsRemoveDest(dest->name, dest->instance,
                       user_data->num_dests,
                       &(user_data->dests));
  }
  else
  {
   /*
    * Add destination to array...
    */

    user_data->num_dests =
        cupsCopyDest(dest, user_data->num_dests,
                     &(user_data->dests));
  }

  return (1);
}

int
my_get_dests(cups_ptype_t type, cups_ptype_t mask,
             cups_dest_t **dests)
{
  my_user_data_t user_data = { 0, NULL };

  if (!cupsEnumDests(CUPS_DEST_FLAGS_NONE, 1000, NULL, type,
                     mask, (cups_dest_cb_t)my_dest_cb,
                     &user_data))
  {
   /*
    * An error occurred, free all of the destinations and
    * return...
    */

    cupsFreeDests(user_data.num_dests, user_data.dests);

    *dests = NULL;

    return (0);
  }

 /*
  * Return the destination array...
  */

  *dests = user_data.dests;

  return (user_data.num_dests);
}

void getMedia(cups_dest_t *dest)
{
	cups_dinfo_t *info = cupsCopyDestInfo(CUPS_HTTP_DEFAULT, dest);

        // IPP QUERY (Resolution):
        char resource[256];
        http_t *http = cupsConnectDest(dest, CUPS_DEST_FLAGS_DEVICE,
                                       30000, NULL, resource,
                                       sizeof(resource), NULL, NULL);

        ipp_t *request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
        const char *printer_uri = cupsGetOption("device-uri",
                                                dest->num_options,
                                                dest->options);

        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                     "printer-uri", NULL, printer_uri);
        const char *requested_attributes[] = { "printer-resolution-default" };
        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                      "requested-attributes", 1, NULL,
                      requested_attributes);

        ipp_t *response = cupsDoRequest(http, request, resource);

        ipp_attribute_t *attr = ippFindAttribute(response, "printer-resolution-default", IPP_TAG_RESOLUTION);
        ipp_tag_t tag = ippGetValueTag(attr);
        int rescount = ippGetCount(attr);
        if( tag != IPP_TAG_RESOLUTION )
        {
            fprintf(stderr, "Didn't get a resolution tag for some reason.\n");
            return;
        }
        for( int x=0; x < rescount; x++ )
        {
            ipp_res_t units;
            int yres;
            int hres = ippGetResolution(attr, x, &yres, &units);

            if (units == IPP_RES_PER_CM)
            {
                hres = (int)(hres * 2.54);
                yres = (int)(yres * 2.54);
            }
            printf("Resolution %d: %d x %d\n", x, hres, yres);
        }
        ippDelete(response);

	cups_size_t size;
	int i;
	int flags = 0;
	int count = cupsGetDestMediaCount(CUPS_HTTP_DEFAULT, dest, info, flags);
	printf("Printer supports %d sizes.\n", count);
	
	for (i = 0; i < count; i ++)
	{
		if (cupsGetDestMediaByIndex(CUPS_HTTP_DEFAULT, dest, info, i, flags, &size))
		{
			printf(" ---------- \n");
			//printf("%s:\n", size.name);
			printf(" Width: %.2fin\n", size.width / 2540.0);
			printf("Length: %.2fin\n", size.length / 2540.0);
			printf("Bottom: %.2fin\n", size.bottom / 2540.0);
			printf("  Left: %.2fin\n", size.left / 2540.0);
			printf(" Right: %.2fin\n", size.right / 2540.0);
			printf("   Top: %.2fin\n", size.top / 2540.0);
		}
	}

        cupsFreeDestInfo(info);
        httpClose(http);
}

int printTo(cups_dest_t *dest, char *pdfdata, size_t length)
{
	int job_id = 0;
	int num_options = 0;
	cups_option_t *options = NULL;
	cups_dinfo_t *info = cupsCopyDestInfo(CUPS_HTTP_DEFAULT, dest);
	
        num_options = cupsAddOption(CUPS_COPIES, "1", num_options, &options);
        //num_options = cupsAddOption(CUPS_MEDIA, CUPS_MEDIA_LETTER, num_options, &options);
        //num_options = cupsAddOption(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT, num_options, &options);
	
	ipp_status_t ret = cupsCreateDestJob(CUPS_HTTP_DEFAULT, dest, info, &job_id, "My Document", num_options, options);
	if( IPP_STATUS_OK == ret )
		printf("Created job: %d\n", job_id);
	else
		printf("Unable to create job: %s\n", cupsLastErrorString());

        if( IPP_STATUS_OK != ret )
        {
            cupsFreeOptions(num_options, options);
            return 1;
        }

        if (cupsStartDestDocument(CUPS_HTTP_DEFAULT, dest, info,
                                  job_id, "filename.pdf", CUPS_FORMAT_PDF, 0, NULL,
                                  1) == HTTP_STATUS_CONTINUE)
        {
            if (cupsWriteRequestData(CUPS_HTTP_DEFAULT, pdfdata, length) != HTTP_STATUS_CONTINUE)
            {
                cupsFreeDestInfo(info);
                cupsFreeOptions(num_options, options);
                return 1;
            }

          if (cupsFinishDestDocument(CUPS_HTTP_DEFAULT, dest, info) == IPP_STATUS_OK)
            puts("Document send succeeded.");
          else
            printf("Document send failed: %s\n", cupsLastErrorString());
        }

        cupsFreeDestInfo(info);
        cupsFreeOptions(num_options, options);

        return 0;
}

int printPDF(const char *destName, char *buf, size_t len)
{
    int ret = 1;
    cups_dest_t *dests;
    int ndests = my_get_dests( 0, 0, &dests );

    for( int x=0; x < ndests; x++ )
    {
            cups_dest_t *dest = &dests[x];
            //const char *model = cupsGetOption("printer-make-and-model",
            const char *model = cupsGetOption("printer-info",
                              dest->num_options,
                              dest->options);

            //printf("Printer: %s\n", model);
            char *isin = strstr( model, destName );
            if( NULL != isin )
            {
                    //getMedia(dest);

                    printTo(dest, buf, len);
                    ret = 0;
                    break;
            }
    }

    cupsFreeDests(ndests, dests);

    return ret;
}

/*
int main(int argc, char **argv)
{
    struct stat stbuf;
    FILE *fp = fopen("image.b64", "rb");
    fstat( fileno(fp), &stbuf );
    size_t len = stbuf.st_size;
    char *buf = malloc( len + 1 );
    fread( buf, 1, len, fp );
    fclose(fp);

    size_t size;
    unsigned char *nbuf = base64_decode(buf, len, &size);
    free( buf );

    char *obuf;
    size_t olen;
    int convret = image2pdf( (7.19 / 2.54), 203, nbuf, size, &obuf, &olen );
    free( nbuf );

    printPDF("CT-S310", obuf, olen);
    free( obuf );

    return 0;
}
*/
