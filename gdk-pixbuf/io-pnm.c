/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GdkPixbuf library - PNM image loader
 *
 * Copyright (C) 1999 Red Hat, Inc.
 *
 * Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *          Michael Fulbright <drmike@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"


#define PNM_BUF_SIZE 4096

#define PNM_FATAL_ERR  -1
#define PNM_SUSPEND     0
#define PNM_OK          1

typedef enum {
	PNM_FORMAT_PGM = 1,
	PNM_FORMAT_PGM_RAW,
	PNM_FORMAT_PPM,
	PNM_FORMAT_PPM_RAW,
	PNM_FORMAT_PBM,
	PNM_FORMAT_PBM_RAW
} PnmFormat;

typedef struct {
	guchar buffer[PNM_BUF_SIZE];
	guchar *byte;
	guint nbytes;
} PnmIOBuffer;

typedef struct {
	ModuleUpdatedNotifyFunc updated_func;
	ModulePreparedNotifyFunc prepared_func;
	gpointer user_data;
	
	GdkPixbuf *pixbuf;
	guchar *pixels;        /* incoming pixel data buffer */
	guchar *dptr;          /* current position in pixbuf */
	
	PnmIOBuffer inbuf;
	
	guint width;
	guint height;
	guint maxval;
	guint rowstride;
	PnmFormat type;
	
	guint output_row;      /* last row to be completed */
	guint output_col;
	gboolean did_prescan;  /* are we in image data yet? */
	gboolean got_header;   /* have we loaded pnm header? */
	
	guint scan_state;
	
} PnmLoaderContext;

GdkPixbuf   *gdk_pixbuf__pnm_image_load          (FILE *f);
gpointer    gdk_pixbuf__pnm_image_begin_load     (ModulePreparedNotifyFunc func, 
						  ModuleUpdatedNotifyFunc func2,
						  ModuleFrameDoneNotifyFunc frame_done_func,
						  ModuleAnimationDoneNotifyFunc anim_done_func,
						  gpointer user_data);
void        gdk_pixbuf__pnm_image_stop_load      (gpointer context);
gboolean    gdk_pixbuf__pnm_image_load_increment (gpointer context, guchar *buf, guint size);

static void explode_bitmap_into_buf              (PnmLoaderContext *context);
static void explode_gray_into_buf                (PnmLoaderContext *context);

/* Destroy notification function for the pixbuf */
static void
free_buffer (guchar *pixels, gpointer data)
{
	g_free (pixels);
}


/* explode bitmap data into rgb components         */
/* we need to know what the row so we can          */
/* do sub-byte expansion (since 1 byte = 8 pixels) */
/* context->dptr MUST point at first byte in incoming data  */
/* which corresponds to first pixel of row y       */
static void
explode_bitmap_into_buf (PnmLoaderContext *context)
{
	gint j;
	guchar *from, *to, data;
	gint bit;
	guchar *dptr;
	gint wid, x, y;
	
	g_return_if_fail (context != NULL);
	g_return_if_fail (context->dptr != NULL);
	
	/* I'm no clever bit-hacker so I'm sure this can be optimized */
	dptr = context->dptr;
	y    = context->output_row;
	wid  = context->width;
	
	from = dptr + ((wid - 1) / 8);
	to   = dptr + (wid - 1) * 3;
/*	bit  = 7 - (((y+1)*wid-1) % 8); */
	bit  = 7 - ((wid-1) % 8);
	
	/* get first byte and align properly */
	data = from[0];
	for (j = 0; j < bit; j++, data >>= 1);
	
	for (x = wid-1; x >= 0; x--) {
/*		g_print ("%c",  (data & 1) ? '*' : ' '); */
		
		to[0] = to[1] = to[2] = (data & 0x01) ? 0x00 : 0xff;
		
		to -= 3;
		bit++;
		
		if (bit > 7) {
			from--;
			data = from[0];
			bit = 0;
		} else {
			data >>= 1;
		}
	}
	
/*	g_print ("\n"); */
}

/* explode gray image row into rgb components in pixbuf */
static void
explode_gray_into_buf (PnmLoaderContext *context)
{
	gint j;
	guchar *from, *to;
	guint w;
	
	g_return_if_fail (context != NULL);
	g_return_if_fail (context->dptr != NULL);
	
	/* Expand grey->colour.  Expand from the end of the
	 * memory down, so we can use the same buffer.
	 */
	w = context->width;
	from = context->dptr + w - 1;
	to = context->dptr + (w - 1) * 3;
	for (j = w - 1; j >= 0; j--) {
		to[0] = from[0];
		to[1] = from[0];
		to[2] = from[0];
		to -= 3;
		from--;
	}
}

/* skip over whitespace and comments in input buffer */
static gint
pnm_skip_whitespace (PnmIOBuffer *inbuf)
{
	register guchar *inptr;
	guchar *inend;
	
	g_return_val_if_fail (inbuf != NULL, PNM_FATAL_ERR);
	g_return_val_if_fail (inbuf->byte != NULL, PNM_FATAL_ERR);
	
	inend = inbuf->byte + inbuf->nbytes;
	inptr = inbuf->byte;
	
	for ( ; inptr < inend; inptr++) {
		if (*inptr == '#') {
			/* in comment - skip to the end of this line */
			for ( ; *inptr != '\n' && inptr < inend; inptr++);
		} else if (!isspace (*inptr)) {
			inbuf->byte = inptr;
			inbuf->nbytes = (guint) (inend - inptr);
			return PNM_OK;
		}
	}
	
	inbuf->byte = inptr;
	inbuf->nbytes = (guint) (inend - inptr);
	
	return PNM_SUSPEND;
}

/* read next number from buffer */
static gint
pnm_read_next_value (PnmIOBuffer *inbuf, guint *value)
{
	register guchar *inptr, *word, *p;
	guchar *inend, buf[128];
	gchar *endptr;
	gint retval;
	
	g_return_val_if_fail (inbuf != NULL, PNM_FATAL_ERR);
	g_return_val_if_fail (inbuf->byte != NULL, PNM_FATAL_ERR);
	g_return_val_if_fail (value != NULL, PNM_FATAL_ERR);
	
	/* skip white space */
	if ((retval = pnm_skip_whitespace (inbuf)) != PNM_OK)
		return retval;
	
	inend = inbuf->byte + inbuf->nbytes;
	inptr = inbuf->byte;
	
	/* copy this pnm 'word' into a temp buffer */
	for (p = inptr, word = buf; (p < inend) && !isspace (*p) && (p - inptr < 128); p++, word++)
		*word = *p;
	*word = '\0';
	
	/* hmmm, there must be more data to this 'word' */
	if (!isspace (*p))
		return PNM_SUSPEND;
	
	/* get the value */
	*value = strtol (buf, &endptr, 10);
	if (*endptr != '\0')
		return PNM_FATAL_ERR;
	
	inbuf->byte = p;
	inbuf->nbytes = (guint) (inend - p);
	
	return PNM_OK;
}

/* returns PNM_OK, PNM_SUSPEND, or PNM_FATAL_ERR */
static gint
pnm_read_header (PnmLoaderContext *context)
{
	PnmIOBuffer *inbuf;
	gint retval;
	
	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);
	
	inbuf = &context->inbuf;
	
	if (!context->type) {
		/* file must start with a 'P' followed by a numeral  */
		/* so loop till we get enough data to determine type */
		if (inbuf->nbytes < 2)
			return PNM_SUSPEND;
		
		if (*inbuf->byte != 'P')
			return PNM_FATAL_ERR;
		
		inbuf->byte++;
		inbuf->nbytes--;
		
		switch (*inbuf->byte) {
		case '1':
			context->type = PNM_FORMAT_PBM;
			break;
		case '2':
			context->type = PNM_FORMAT_PGM;
			break;
		case '3':
			context->type = PNM_FORMAT_PPM;
			break;
		case '4':
			context->type = PNM_FORMAT_PBM_RAW;
			break;
		case '5':
			context->type = PNM_FORMAT_PGM_RAW;
			break;
		case '6':
			context->type = PNM_FORMAT_PPM_RAW;
			break;
		default:
			return PNM_FATAL_ERR;
		}
		
		if (!inbuf->nbytes)
			return PNM_SUSPEND;
		
		inbuf->byte++;
		inbuf->nbytes--;
	}
	
	if (!context->width) {
		/* read the pixmap width */
		guint width = 0;
		
		retval = pnm_read_next_value (inbuf, &width);
		
		if (retval != PNM_OK)
			return retval;
		
		if (!width)
			return PNM_FATAL_ERR;
		
		context->width = width;
	}
	
	if (!context->height) {
		/* read the pixmap height */
		guint height = 0;
		
		retval = pnm_read_next_value (inbuf, &height);
		
		if (retval != PNM_OK)
			return retval;
		
		if (!height)
			return PNM_FATAL_ERR;
		
		context->height = height;
	}
	
	switch (context->type) {
	case PNM_FORMAT_PPM:
	case PNM_FORMAT_PPM_RAW:
	case PNM_FORMAT_PGM:
	case PNM_FORMAT_PGM_RAW:
		if (!context->maxval) {
			retval = pnm_read_next_value (inbuf, &context->maxval);
			
			if (retval != PNM_OK)
				return retval;
			
			if (context->maxval == 0)
				return PNM_FATAL_ERR;
		}
		break;
	default:
		break;
	}
	
	return PNM_OK;
}

static gint
pnm_read_raw_scanline (PnmLoaderContext *context)
{
	PnmIOBuffer *inbuf;
	guint numbytes, offset;
	guint numpix = 0;
	guchar *dest;
	guint i;
	
	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);
	
	inbuf = &context->inbuf;
	
	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
		numpix = inbuf->nbytes * 8;
		break;
	case PNM_FORMAT_PGM_RAW:
		numpix = inbuf->nbytes;
		break;
	case PNM_FORMAT_PPM_RAW:
		numpix = inbuf->nbytes / 3;
		break;
	default:
		g_warning ("io-pnm.c: Illegal raw pnm type!\n");
		return PNM_FATAL_ERR;
	}
	
	numpix = MIN (numpix, context->width - context->output_col);
	
	if (!numpix)
		return PNM_SUSPEND;
	
	context->dptr = context->pixels + context->output_row * context->rowstride;
	
	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
		numbytes = (numpix / 8) + ((numpix % 8) ? 1 : 0);
		offset = context->output_col / 8;
		break;
	case PNM_FORMAT_PGM_RAW:
		numbytes = numpix;
		offset = context->output_col;
		break;
	case PNM_FORMAT_PPM_RAW:
		numbytes = numpix * 3;
		offset = context->output_col * 3;
		break;
	default:
		g_warning ("io-pnm.c: Illegal raw pnm type!\n");
		return PNM_FATAL_ERR;
	}
	
	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
		dest = context->dptr + offset;		
		memcpy (dest, inbuf->byte, numbytes);
		break;
	case PNM_FORMAT_PGM_RAW:
	case PNM_FORMAT_PPM_RAW:
		dest = context->dptr + offset;
		
		if (context->maxval == 255) {
			/* special-case optimization */
			memcpy (dest, inbuf->byte, numbytes);
		} else {
			for (i = 0; i < numbytes; i++) {
				guchar *byte = inbuf->byte + i;
				
				/* scale the color to an 8-bit color depth */
				if (*byte > context->maxval)
					*dest++ = 255;
				else
					*dest++ = (guchar) (255 * *byte / context->maxval);
			}
		}
		break;
	default:
		g_warning ("Invalid raw pnm format!");
	}
	
	inbuf->byte += numbytes;
	inbuf->nbytes -= numbytes;
	
	context->output_col += numpix;
	if (context->output_col == context->width) {
		if (context->type == PNM_FORMAT_PBM_RAW)
			explode_bitmap_into_buf (context);
		else if (context->type == PNM_FORMAT_PGM_RAW)
			explode_gray_into_buf (context);
		
		context->output_col = 0;
		context->output_row++;
	} else {
		return PNM_SUSPEND;
	}
	
	return PNM_OK;
}

static gint
pnm_read_ascii_scanline (PnmLoaderContext *context)
{
	PnmIOBuffer *inbuf;
	guint offset;
	guint value, numval, i;
	guchar data;
	guchar mask;
	guchar *dptr;
	gint retval;
	
	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);
	
	data = mask = 0;
	
	inbuf = &context->inbuf;
	
	context->dptr = context->pixels + context->output_row * context->rowstride;
	
	switch (context->type) {
	case PNM_FORMAT_PBM:
		numval = MIN (8, context->width - context->output_col);
		offset = context->output_col / 8;
		break;
	case PNM_FORMAT_PGM:
		numval = 1;
		offset = context->output_col;
		break;
	case PNM_FORMAT_PPM:
		numval = 3;
		offset = context->output_col * 3;
		break;
		
	default:
		g_warning ("Can't happen\n");
		return PNM_FATAL_ERR;
	}
	
	dptr = context->dptr + offset + context->scan_state;
	
	while (TRUE) {
		if (context->type == PNM_FORMAT_PBM) {
			mask = 0x80;
			data = 0;
			numval = MIN (8, context->width - context->output_col);
		}
		
		for (i = context->scan_state; i < numval; i++) {
			retval = pnm_read_next_value (inbuf, &value);
			if (retval != PNM_OK) {
				/* save state and return */
				context->scan_state = i;
				return retval;
			}
			
			switch (context->type) {
			case PNM_FORMAT_PBM:
				if (value)
					data |= mask;
				mask >>= 1;
				
				break;
			case PNM_FORMAT_PGM:
			case PNM_FORMAT_PPM:
				/* scale the color to an 8-bit color depth */
				if (value > context->maxval)
					*dptr++ = 255;
				else
					*dptr++ = (guchar)(255 * value / context->maxval);
				break;
			default:
				g_warning ("io-pnm.c: Illegal ascii pnm type!\n");
				break;
			}
		}
		
		context->scan_state = 0;
		
		if (context->type == PNM_FORMAT_PBM) {
			*dptr++ = data;
			context->output_col += numval;
		} else {
			context->output_col++;
		}
		
		if (context->output_col == context->width) {
			if (context->type == PNM_FORMAT_PBM)
				explode_bitmap_into_buf (context);
			else if (context->type == PNM_FORMAT_PGM)
				explode_gray_into_buf (context);
			
			context->output_col = 0;
			context->output_row++;
			break;
		}
	}
	
	return PNM_OK;
}

/* returns 1 if a scanline was converted, 0 means we ran out of data */
static gint
pnm_read_scanline (PnmLoaderContext *context)
{
	gint retval;
	
	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);
	
	/* read in image data */
	/* for raw formats this is trivial */
	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
	case PNM_FORMAT_PGM_RAW:
	case PNM_FORMAT_PPM_RAW:
		retval = pnm_read_raw_scanline (context);
		if (retval != PNM_OK)
			return retval;
		break;
	case PNM_FORMAT_PBM:
	case PNM_FORMAT_PGM:
	case PNM_FORMAT_PPM:
		retval = pnm_read_ascii_scanline (context);
		if (retval != PNM_OK)
			return retval;
		break;
	default:
		g_warning ("Cannot load these image types (yet)\n");
		return PNM_FATAL_ERR;
	}
	
	return PNM_OK;
}

/* Shared library entry point */
GdkPixbuf *
gdk_pixbuf__pnm_image_load (FILE *f)
{
	PnmLoaderContext context;
	PnmIOBuffer *inbuf;
	gint nbytes;
	gint retval;
	
	/* pretend to be doing progressive loading */
	context.updated_func = NULL;
	context.prepared_func = NULL;
	context.user_data = NULL;
	context.type = 0;
	context.inbuf.nbytes = 0;
	context.inbuf.byte = NULL;
	context.width = 0;
	context.height = 0;
	context.maxval = 0;
	context.pixels = NULL;
	context.pixbuf = NULL;
	context.got_header = FALSE;
	context.did_prescan = FALSE;
	context.scan_state = 0;
	
	inbuf = &context.inbuf;
	
	while (TRUE) {
		guint num_to_read;
		
		/* keep buffer as full as possible */
		num_to_read = PNM_BUF_SIZE - inbuf->nbytes;
		
		if (inbuf->byte != NULL && inbuf->nbytes > 0)
			memmove (inbuf->buffer, inbuf->byte, inbuf->nbytes);
		
		nbytes = fread (inbuf->buffer + inbuf->nbytes, 1, num_to_read, f);
		
		/* error checking */
		if (nbytes == 0) {
			/* we ran out of data because of a read error or premature file end */
			if (context.pixbuf)
				gdk_pixbuf_unref (context.pixbuf);

			if (ferror (f))
				g_warning ("io-pnm.c: Error reading image file.\n");
			else
				g_warning ("io-pnm.c: Ran out of data.\n");

			return NULL;
		}
		
		inbuf->nbytes += nbytes;
		inbuf->byte = inbuf->buffer;
		
		/* get header if needed */
		if (!context.got_header) {
			retval = pnm_read_header (&context);
			if (retval == PNM_FATAL_ERR)
				return NULL;
			else if (retval == PNM_SUSPEND)
				continue;
			
			context.got_header = TRUE;
		}
		
		/* scan until we hit image data */
		if (!context.did_prescan) {
			switch (context.type) {
			case PNM_FORMAT_PBM_RAW:
			case PNM_FORMAT_PGM_RAW:
			case PNM_FORMAT_PPM_RAW:
				/* raw formats only allow one whitespace after the header */
				if (inbuf->nbytes <= 0 || !isspace (*inbuf->byte))
					continue;

				inbuf->nbytes--;
				inbuf->byte++;
				break;

			default:
				retval = pnm_skip_whitespace (inbuf);
			
				if (retval == PNM_FATAL_ERR)
					return FALSE;
				else if (retval == PNM_SUSPEND)
					continue;
			}

			context.did_prescan = TRUE;
			context.output_row = 0;
			context.output_col = 0;
			
			context.rowstride = context.width * 3;
			context.pixels = g_malloc (context.height * context.width * 3);
			
			if (!context.pixels) {
				/* Failed to allocate memory */
				g_warning ("Couldn't allocate pixel buf");
			}
		}
		
		/* if we got here we're reading image data */
		while (context.output_row < context.height) {
			retval = pnm_read_scanline (&context);
			
			if (retval == PNM_SUSPEND) {
				break;
			} else if (retval == PNM_FATAL_ERR) {
				if (context.pixbuf)
					gdk_pixbuf_unref (context.pixbuf);
				g_warning ("io-pnm.c: error reading rows..\n");
				return NULL;
			}
		}
		
		if (context.output_row < context.height)
			continue;
		else
			break;
	}
	
	return gdk_pixbuf_new_from_data (context.pixels, GDK_COLORSPACE_RGB, FALSE, 8,
					 context.width, context.height, 
					 context.width * 3, free_buffer, NULL);

}

/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

gpointer
gdk_pixbuf__pnm_image_begin_load (ModulePreparedNotifyFunc prepared_func, 
				  ModuleUpdatedNotifyFunc  updated_func,
				  ModuleFrameDoneNotifyFunc frame_done_func,
				  ModuleAnimationDoneNotifyFunc anim_done_func,
				  gpointer user_data)
{
	PnmLoaderContext *context;
	
	context = g_new0 (PnmLoaderContext, 1);
	context->prepared_func = prepared_func;
	context->updated_func  = updated_func;
	context->user_data = user_data;
	context->width = 0;
	context->height = 0;
	context->maxval = 0;
	context->pixbuf = NULL;
	context->pixels = NULL;
	context->got_header = FALSE;
	context->did_prescan = FALSE;
	context->scan_state = 0;
	
	context->inbuf.nbytes = 0;
	context->inbuf.byte  = NULL;
	
	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
void
gdk_pixbuf__pnm_image_stop_load (gpointer data)
{
	PnmLoaderContext *context = (PnmLoaderContext *) data;
	
	g_return_if_fail (context != NULL);
	
	if (context->pixbuf)
		gdk_pixbuf_unref (context->pixbuf);
	
	g_free (context);
}

/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
gboolean
gdk_pixbuf__pnm_image_load_increment (gpointer data, guchar *buf, guint size)
{
	PnmLoaderContext *context = (PnmLoaderContext *)data;
	PnmIOBuffer *inbuf;
	guchar *old_byte;
	guint old_nbytes;
	guchar *bufhd;
	guint num_left, spinguard;
	gint retval;
	
	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);
	
	bufhd = buf;
	inbuf = &context->inbuf;
	old_nbytes = inbuf->nbytes;
	old_byte  = inbuf->byte;
	
	num_left = size;
	spinguard = 0;
	while (TRUE) {
		guint num_to_copy;
		
		/* keep buffer as full as possible */
		num_to_copy = MIN (PNM_BUF_SIZE - inbuf->nbytes, num_left);
		
		if (num_to_copy == 0)
			spinguard++;
		
		if (spinguard > 1)
			return TRUE;
		
		if (inbuf->byte != NULL && inbuf->nbytes > 0)
			memmove (inbuf->buffer, inbuf->byte, inbuf->nbytes);
		
		memcpy (inbuf->buffer + inbuf->nbytes, bufhd, num_to_copy);
		bufhd += num_to_copy;
		inbuf->nbytes += num_to_copy;
		inbuf->byte = inbuf->buffer;
		num_left -= num_to_copy;
		
		/* ran out of data and we haven't exited main loop */
		if (inbuf->nbytes == 0)
			return TRUE;
		
		/* get header if needed */
		if (!context->got_header) {
			retval = pnm_read_header (context);
			
			if (retval == PNM_FATAL_ERR)
				return FALSE;
			else if (retval == PNM_SUSPEND)
				continue;
			
			context->got_header = TRUE;
		}
		
		/* scan until we hit image data */
		if (!context->did_prescan) {
			switch (context->type) {
			case PNM_FORMAT_PBM_RAW:
			case PNM_FORMAT_PGM_RAW:
			case PNM_FORMAT_PPM_RAW:
				/* raw formats only allow one whitespace after the header */
				if (inbuf->nbytes <= 0 || !isspace (*inbuf->byte))
					continue;

				inbuf->nbytes--;
				inbuf->byte++;
				break;

			default:
				retval = pnm_skip_whitespace (inbuf);
			
				if (retval == PNM_FATAL_ERR)
					return FALSE;
				else if (retval == PNM_SUSPEND)
					continue;
			}
						
			context->did_prescan = TRUE;
			context->output_row = 0;
			context->output_col = 0;
			
			context->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
							  FALSE,
							  8, 
							  context->width,
							  context->height);
			
			if (context->pixbuf == NULL)
				return FALSE;
			
			context->pixels = context->pixbuf->pixels;
			context->rowstride = context->pixbuf->rowstride;
			
			/* Notify the client that we are ready to go */
			(* context->prepared_func) (context->pixbuf,
						    context->user_data);
		}
		
		/* if we got here we're reading image data */
		while (context->output_row < context->height) {
			retval = pnm_read_scanline (context);
			
			if (retval == PNM_SUSPEND) {
				break;
			} else if (retval == PNM_FATAL_ERR) {
				if (context->pixbuf)
					gdk_pixbuf_unref (context->pixbuf);
				g_warning ("io-pnm.c: error reading rows.\n");
				return FALSE;
			} else if (retval == PNM_OK) {	
				/* send updated signal */
				(* context->updated_func) (context->pixbuf,
							   0, 
							   context->output_row-1,
							   context->width, 
							   1,
							   context->user_data);
			}
		}
		
		if (context->output_row < context->height)
			continue;
		else
			break;
	}
	
	return TRUE;
}
