/*
 * Copyright (C) 2017 Timothy Bourke (tim@tbrk.org)
 *
 * The maildir_parse_flags and maildir_parse_message functions are taken
 * directly from the mh.c file in the Mutt source code since they are not
 * exported. The mh.c file is released under the GPL 2 and
 * Copyright (C) 1996-2002,2007,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2010,2013 Michael R. Elkins <me@mutt.org>
 *
 * The handler functions are adapted directly from the handler.c file in
 * the Mutt source code. The handler.c file is released under the GPL 2 and
 * Copyright (C) 1996-2000,2002,2010,2013 Michael R. Elkins <me@mutt.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "muttiface.h"

#define MAIN_C 1
#include "config.h"

#include "mutt.h"
#include "mutt_curses.h"
#include "copy.h"
#include "charset.h"
#include "rfc2047.h"

#include <stdlib.h>
#include <libgen.h>	// dirname, basename
#include <unistd.h>	// fork, getopt
#include <string.h>	// strspn, strcspn

#include "fwrapdata.h"
#include "entities.h"

// TODO: Robust handling of errors
// TODO: Localize header strings; can mutt do this?
// TODO: Localize error messages; use cocoa features?

void (*mutt_error) (const char *, ...) = mutt_nocurses_error;

void no_mutt_message(const char * msg, ...)
{
};

void mutt_exit (int code)
{
  exit (code);
}

/**
 * Generate a textual preview of a mail message.
 */

static char *chomp_maildir_ext(char *path)
{
    size_t l = strlen(path);
    char *last = path + (l - 4);

    if (l > 4 && ((strcmp(last, "/cur") == 0)
		  || (strcmp(last, "/new") == 0)
		  || (strcmp(last, "/tmp") == 0))) {
	*last = '\0';
	return (last + 1);
    }

    return NULL;
}

struct copy_of_mh_data
{
  time_t mtime_cur;
  mode_t mh_umask;
};

// adapted from mx_open_mailbox()
static CONTEXT *init_context(void)
{
    int rc;
    CONTEXT *ctx = safe_malloc(sizeof(CONTEXT));

    memset(ctx, 0, sizeof(CONTEXT));

    ctx->msgnotreadyet = -1;
    ctx->collapsed = 0;

    for (rc=0; rc < RIGHTSMAX; rc++)
	mutt_bit_set(ctx->rights, rc);

    ctx->quiet = 1;
    ctx->readonly = 1;
    ctx->peekonly = 1;
    ctx->append = 1;
    ctx->magic = MUTT_MAILDIR;
    ctx->mx_ops = mx_get_ops(ctx->magic);

    mutt_make_label_hash(ctx);
    ctx->data = safe_calloc(sizeof(struct copy_of_mh_data), 1);

    return ctx;
}

static void set_context_path(CONTEXT *ctx, char *path)
{
    mutt_str_replace(&ctx->path, path);
    FREE(&ctx->realpath);

    if (! (ctx->realpath = realpath(ctx->path, NULL)) )
	ctx->realpath = safe_strdup(ctx->path);
}

// adapted from mx_fastclose_mailbox
static void free_context(CONTEXT **pctx)
{
    int i;

    CONTEXT *ctx = *pctx;

    if(!ctx) 
	return;

    if (ctx->mx_ops)
	ctx->mx_ops->close(ctx);

    if (ctx->subj_hash)
	hash_destroy (&ctx->subj_hash, NULL);

    if (ctx->id_hash)
	hash_destroy (&ctx->id_hash, NULL);

    hash_destroy(&ctx->label_hash, NULL);
    mutt_clear_threads(ctx);

    for (i = 0; i < ctx->msgcount; i++)
	mutt_free_header(&(ctx->hdrs[i]));

    FREE (&ctx->hdrs);
    FREE (&ctx->v2r);
    FREE (&ctx->path);
    FREE (&ctx->realpath);
    FREE (&ctx->pattern);
    /*
    if (ctx->limit_pattern) 
	mutt_pattern_free(&ctx->limit_pattern);
    */

    safe_fclose(&ctx->fp);

    FREE(pctx);
}

// copied directly from mutt: mh.c
static void maildir_parse_flags (HEADER * h, const char *path)
{
  char *p, *q = NULL;

  h->flagged = 0;
  h->read = 0;
  h->replied = 0;

  if ((p = strrchr (path, ':')) != NULL && mutt_strncmp (p + 1, "2,", 2) == 0)
  {
    p += 3;
    
    mutt_str_replace (&h->maildir_flags, p);
    q = h->maildir_flags;

    while (*p)
    {
      switch (*p)
      {
      case 'F':

	h->flagged = 1;
	break;

      case 'S':		/* seen */

	h->read = 1;
	break;

      case 'R':		/* replied */

	h->replied = 1;
	break;

      case 'T':		/* trashed */
	if (!h->flagged || !option(OPTFLAGSAFE))
	{
	  h->trash = 1;
	  h->deleted = 1;
	}
	break;
      
      default:
	*q++ = *p;
	break;
      }
      p++;
    }
  }
  
  if (q == h->maildir_flags)
    FREE (&h->maildir_flags);
  else if (q)
    *q = '\0';
}

// copied directly from mutt: mh.c
static HEADER *maildir_parse_message (int magic, const char *fname,
				      int is_old, HEADER * _h)
{
  FILE *f;
  HEADER *h = _h;
  struct stat st;

  if ((f = fopen(fname, "r")) != NULL)
  {
    if (!h)
      h = mutt_new_header ();
    h->env = mutt_read_rfc822_header (f, h, 0, 0);

    fstat (fileno (f), &st);
    safe_fclose (&f);

    if (!h->received)
      h->received = h->date_sent;

    /* always update the length since we have fresh information available. */
    h->content->length = st.st_size - h->content->offset;

    h->index = -1;

    if (magic == MUTT_MAILDIR)
    {
      /* 
       * maildir stores its flags in the filename, so ignore the
       * flags in the header of the message 
       */

      h->old = is_old;
      maildir_parse_flags (h, fname);
    }
    return h;
  }
  return NULL;
}

static int read_message(CONTEXT *ctx, char *subdir, char *msgname)
{
    HEADER *hdr = NULL;
    int r = -1;
    char file[_POSIX_PATH_MAX];
    char path[_POSIX_PATH_MAX];

    if (subdir)
	snprintf(file, sizeof(file), "%s/%s", subdir, msgname);
    else
	strncpy(file, msgname, sizeof(file));

    snprintf(path, sizeof(path), "%s/%s", ctx->path, file);

    hdr = maildir_parse_message(ctx->magic, path, 1, NULL);
    if (hdr == NULL) return -1;
    mutt_str_replace(&hdr->path, file);

    if (ctx->msgcount == ctx->hdrmax)
	mx_alloc_memory(ctx);

    ctx->hdrs[ctx->msgcount] = hdr;
    ctx->hdrs[ctx->msgcount]->index = ctx->msgcount;
    ctx->size += hdr->content->length
		    + hdr->content->offset - hdr->content->hdr_offset;
    r = ctx->msgcount;
    ctx->msgcount++;

    mutt_parse_mime_message(ctx, hdr);
    return r;
}

static void free_message(CONTEXT *ctx)
{
    if (ctx->msgcount > 0)
	mutt_free_header(&(ctx->hdrs[--ctx->msgcount]));
}

#define HTML_START \
  "<!doctype html>\n"							    \
  "<html>\n"								    \
  "<head>\n"								    \
  "  <meta charset=\"utf-8\">\n"					    \
  "  <title>Email Preview</title>\n"					    \
  "  <link rel=\"stylesheet\" href=\"cid:email.css\" type=\"text/css\" />\n"\
  "</head>\n"								    \

static const char *text_is_unsupported = " is unsupported  --]";

#define APPEND_LITERAL(data, bytes) \
    CFDataAppendBytes(data, (const UInt8 *)bytes, sizeof(bytes) - 1);

static void encode_html(CFMutableDataRef data, char *buf, int add_brs)
{
    int pre_space = 0;

    while (*buf) {
	switch (*buf) {
	case '<':
	    APPEND_LITERAL(data, "&lt;");
	    break;
	case '>':
	    APPEND_LITERAL(data, "&gt;");
	    break;
	case '&':
	    APPEND_LITERAL(data, "&amp;");
	    break;
	case '\n':
	    if (add_brs && !pre_space)
		APPEND_LITERAL(data, "<br/>");
	default:
	    CFDataAppendBytes(data, (UInt8 *)buf, 1);
	    break;
	}
	pre_space = (*buf == ' ');
	++buf;
    }
}

static int line_quote_level(const char *line)
{
    int level = 0;

    for ( ; *line; ++line) {
	if (*line == '>')
	    ++level;
	else if (!isspace(*line))
	    break;
    }

    return level;
}

// caller becomes owner of the returned data
void mutt_to_html(struct mutt_to_html_args *args)
{
    FILE *fin = fdopen(args->fdin, "r");
    CFMutableDataRef buf = CFDataCreateMutable(NULL, 0);
    char *line = NULL, *line2;
    char keyname[STRING];
    size_t len = 0;
    ssize_t read;
    int field_open = 0;
    unsigned long b;
    int quote_level;
    size_t len_attmark = strlen(AttachmentMarker);
    static size_t len_text_is_unsupported = 0;
    int has_header = 0, has_body = 0;

    args->dout = NULL;
    if (!fin) return;
    
    APPEND_LITERAL(buf, HTML_START);
    snprintf(keyname, sizeof(keyname),
	     "<body class=\"%s\">\n", args->body_class);
    CFDataAppendBytes(buf, (UInt8 *)keyname, strlen(keyname));
    APPEND_LITERAL(buf, "<table class=\"headers\">");

    while ((read = getline(&line, &len, fin)) != -1) {
	if (line[0] == '\n') break;

	if (line[0] == ' ') {
	    encode_html(buf, line + 1, 0);
	    continue;
	}

	if (field_open)
	    APPEND_LITERAL(buf, "</td></tr>\n");

	b = strcspn(line, ":");

	if (line[b]) {
	    line[b] = '\0';

	    line2 = line + b + 1;
	    line2[strcspn(line2, "\n")] = '\0';

	    strncpy(keyname, line, sizeof(keyname));
	    mutt_strlower(keyname);

	    APPEND_LITERAL(buf, "<tr class=\"");
	    CFDataAppendBytes(buf, (UInt8 *)keyname, strlen(keyname));
	    APPEND_LITERAL(buf, "\"><td class=\"key\">");
	    CFDataAppendBytes(buf, (UInt8 *)line, b);
	    APPEND_LITERAL(buf, ":</td><td class=\"value\">");

	    field_open = 1;
	    has_header = 1;
	    encode_html(buf, line2 + strspn(line2, " \t"), 0);
	} else {
	    field_open = 0;
	}
    }

    if (field_open)
	APPEND_LITERAL(buf, "</td></tr>\n");
    APPEND_LITERAL(buf, "</table>\n");

    APPEND_LITERAL(buf, "<div class=\"messagebody\">\n");
    while ((read = getline(&line, &len, fin)) != -1) {
	if (strncmp(AttachmentMarker, line, len_attmark) == 0) {
	    line[strcspn(line, "\n")] = '\0';
	    if (len_text_is_unsupported == 0)
		len_text_is_unsupported = strlen(text_is_unsupported);
	    b = strlen(line) - len_text_is_unsupported;

	    if (b > 0 && strcmp(line + b, text_is_unsupported) != 0) {
		APPEND_LITERAL(buf, "<span class=\"attachmsg\">");
		encode_html(buf, line + len_attmark, 1);
		APPEND_LITERAL(buf, "</span><br/>\n");
	    }
	} else if ((quote_level = line_quote_level(line)) > 0) {
	    line[strcspn(line, "\n")] = '\0';
	    snprintf(keyname, sizeof(keyname),
		    "<span class=\"quoted quoted%d\">", quote_level - 1);
	    CFDataAppendBytes(buf, (UInt8 *)keyname, strlen(keyname));
	    encode_html(buf, line, 1);
	    APPEND_LITERAL(buf, "</span><br/>\n");
	} else {
	    encode_html(buf, line, 1);
	}
	has_body = 1;
    }

    fclose(fin);

    if (!has_body)
	APPEND_LITERAL(buf,
		"<div class=\"nobody\">(This message is empty.)</div>");

    APPEND_LITERAL(buf, "</div>\n");
    APPEND_LITERAL(buf, "</body></html>\n");

    if (line)
	free(line);

    if (!has_header) {
	CFRelease(buf);
    } else {
	args->dout = (CFDataRef)buf;
    }

    if (args->sync)
	dispatch_semaphore_signal(args->sync);
}

char **envlist;

void init_mutt_iface(char **environ) /* , char *configpath) */
{
    mutt_message = no_mutt_message;

    /* Init envlist */
    {
	char **srcp, **dstp;
	int count = 0;
	for (srcp = environ; srcp && *srcp; srcp++)
	    count++;
	envlist = safe_calloc(count+1, sizeof(char *));
	for (srcp = environ, dstp = envlist; srcp && *srcp; srcp++, dstp++)
	    *dstp = safe_strdup(*srcp);
    }

    memset(Options, 0, sizeof (Options));
    memset(QuadOptions, 0, sizeof (QuadOptions));

    set_option(OPTNOCURSES);
    set_option(OPTVIEWATTACH);
    mutt_init(0, NULL);
    mutt_init_windows ();

    setlocale (LC_ALL, "");
    mutt_str_replace(&Charset, "utf-8");
    mutt_set_charset(Charset);

    Context = init_context();
}

void free_mutt_iface(void)
{
    free_context(&Context);
    mutt_free_opts ();
    mutt_free_windows ();
}

int cat_mutt_message(char *msgpath, FILE *fout)
{
    char *path, *msgname, *subdir;
    int msg;

    msgname = safe_strdup(basename(msgpath));
    path = safe_strdup(dirname(msgpath));
    subdir = safe_strdup(chomp_maildir_ext(path));

    set_context_path(Context, path);

    msg = read_message(Context, subdir, msgname);
    if (msg < 0) {
	fprintf(stderr, "failed to read message!\n");
	exit(-2);
    }
    mx_update_context(Context, 1);

    mutt_copy_message(fout, Context, Context->hdrs[msg],
	    MUTT_CM_DECODE | MUTT_CM_DISPLAY | MUTT_CM_CHARCONV,
	    CH_DECODE | CH_NOLEN | CH_WEED | CH_REORDER | CH_WEED_DELIVERED);

    free_message(Context);

    FREE(&msgname);
    FREE(&path);
    FREE(&subdir);

    return 0;
}


/** Handlers to extract plain text parts from mail for indexing.
 *
 *  Adapted from Mutt's handler.c functions.
 */

/* The following two enumerations are duplicated from Mutt's mime.h to avoid
 * the error:
 *  ./mutt/mime.h:71:20: error: redeclaration of 'BodyTypes' with a
 *  different type: 'const char *[]' vs 'const char *const [9]'
 *	extern const char *BodyTypes[];
 */

/* Content-Type */
enum
{
  TYPEOTHER,
  TYPEAUDIO,
  TYPEAPPLICATION,
  TYPEIMAGE,
  TYPEMESSAGE,
  TYPEMODEL,
  TYPEMULTIPART,
  TYPETEXT,
  TYPEVIDEO,
  TYPEANY
};

/* Content-Transfer-Encoding */
enum
{
  ENCOTHER,
  ENC7BIT,
  ENC8BIT,
  ENCQUOTEDPRINTABLE,
  ENCBASE64,
  ENCBINARY,
  ENCUUENCODED
};



LOFF_T streamlen(FILE *stream)
{
    LOFF_T offset = ftello(stream);
    LOFF_T length;

    fseek(stream, 0, SEEK_END);
    length = ftello(stream);
    fseek(stream, offset, SEEK_SET);

    return length;
}

#define TXTHTML     1
#define TXTENRICHED 2
#define TXTPLAIN    3

typedef void (*handler_t) (BODY *, STATE *);

static void body_handler(BODY *b, STATE *s);

#define WORD_TOO_BIG 50

static void text_plain_handler(BODY *b, STATE *s)
{
    char *buf = NULL, *cur = NULL;
    int toobig = 1;
    size_t sz = 0;

    while ((buf = mutt_read_line(buf, &sz, s->fpin, NULL, 0)))
    {
	for (cur = buf; *cur; ++cur) {
	    if (isspace(*cur)) {
		toobig = 0;
		break;
	    }
	}

	// skip long lines that contain no spaces
	// (no point indexing misinterpreted data)
	if (!toobig || sz < WORD_TOO_BIG)
	    state_puts(buf, s);

	state_putc('\n', s);
    }

    FREE (&buf);
}

static void text_html_handler(BODY *b, STATE *s)
{
    char *buf = NULL;
    char *cur, *to;
    size_t sz = 0;
    int intag = 0, skipws = 1;
    int cur_is_space;
    char entity[8];
    int wordsize = 0; // skip long sequences without spaces

    // rough-and-ready tag skipping and entity conversion
    while ((buf = mutt_read_line(buf, &sz, s->fpin, NULL, MUTT_EOL)))
    {
	for (cur = buf; *cur;) {
	    if (intag) {
		intag = (*cur != '>');
		cur++;
	    } else {
		cur_is_space = isspace(*cur);
		if (skipws && cur_is_space) {
		    cur++;
		    continue;
		}

		if (*cur == '<') {
		    intag = 1;
		    cur++;
		    continue;
		} else if (*cur == '&') {
		    to = entity;
		    if (parse_entity(cur, &to, (const char **)&cur)) {
			*to = '\0';
			state_puts(entity, s);
		    } else {
			state_putc(*cur, s);
			cur++;
		    }
		} else {
		    if (++wordsize >= WORD_TOO_BIG) break;
		    state_putc(*cur, s);
		    cur++;
		}
		if (cur_is_space) {
		    skipws = 0;
		    wordsize= 0;
		}
	    }
	}
    }
    state_putc('\n', s);

    FREE (&buf);
}

static void external_body_handler(BODY *a, STATE *s)
{
    if (a->parts && a->parts->filename) {
	fputc('\n', s->fpout);
	fputs(a->parts->filename, s->fpout);
	fputc('\n', s->fpout);
    }
}

/* handles message/rfc822 body parts */
static void message_handler(BODY *a, STATE *s)
{
    BODY *b;
    LOFF_T off_start;

    off_start = ftello(s->fpin);
    if (a->encoding == ENCBASE64
	|| a->encoding == ENCQUOTEDPRINTABLE
	|| a->encoding == ENCUUENCODED)
    {
	b = mutt_new_body();
	b->length = streamlen(s->fpin);
	b->parts = mutt_parse_messageRFC822(s->fpin, b);
	if (b->parts)
	    body_handler(b->parts, s);
	mutt_free_body (&b);
    }
    else if (a->parts) {
	body_handler(a->parts, s);
    }
}

static void alternative_handler(BODY *a, STATE *s)
{
    BODY *choice = NULL;
    BODY *b;
    int type = 0;
    int mustfree = 0;

    if (a->encoding == ENCBASE64
	|| a->encoding == ENCQUOTEDPRINTABLE
	|| a->encoding == ENCUUENCODED)
    {
	mustfree = 1;
	b = mutt_new_body();
	b->length = (long)streamlen(s->fpin);
	b->parts = mutt_parse_multipart(s->fpin,
	    mutt_get_parameter("boundary", a->parameter),
	    b->length, ascii_strcasecmp("digest", a->subtype) == 0);
    } else
	b = a;

    a = b;

    /* look for a text entry */
    if (!choice)
    {
	if (a && a->parts)
	    b = a->parts;
	else
	    b = a;

	while (b)
	{
	    if (b->type == TYPETEXT)
	    {
		if (!ascii_strcasecmp("plain", b->subtype) && type <= TXTPLAIN)
		{
		    choice = b;
		    type = TXTPLAIN;
		}
		else if (!ascii_strcasecmp("enriched", b->subtype)
			 && type <= TXTENRICHED)
		{
		    choice = b;
		    type = TXTENRICHED;
		}
		else if(!ascii_strcasecmp("html", b->subtype)
			&& type <= TXTHTML)
		{
		    choice = b;
		    type = TXTHTML;
		}
	    }
	    b = b->next;
	}
    }

    /* Finally, look for other possibilities */
    if (!choice)
    {
	if (a && a->parts) 
	    b = a->parts;
	else
	    b = a;

	while (b)
	{
	    if (mutt_can_decode(b))
		choice = b;
	    b = b->next;
	}
    }

    if (choice)
	body_handler(choice, s);

    if (mustfree)
	mutt_free_body(&a);
}

static void signed_handler(BODY *a, STATE *s)
{
    body_handler(a->parts, s);
}

static void multipart_handler(BODY *a, STATE *s)
{
    BODY *b, *p;
    int count;
    int mustfree = 0;

    if (a->encoding == ENCBASE64
	|| a->encoding == ENCQUOTEDPRINTABLE
	|| a->encoding == ENCUUENCODED)
    {
	b = mutt_new_body();
	mustfree = 1;
	b->length = (long)streamlen(s->fpin);
	b->parts = mutt_parse_multipart(s->fpin,
	    mutt_get_parameter("boundary", a->parameter),
	    b->length, ascii_strcasecmp("digest", a->subtype) == 0);
    }
    else
	b = a;

    for (p = b->parts, count = 1; p; p = p->next, count++)
    {
	body_handler(p, s);
    }

    if (mustfree)
	mutt_free_body (&b);
}
 
static void
run_decode_and_handler(BODY *b, STATE *s, handler_t handler, int plaintext)
{
    int origType;
    FILE *fp = NULL;
    FILE *fp2 = NULL;
    size_t tmplength = 0;
    LOFF_T tmpoffset = 0;
    int decode = 0;

    fseeko(s->fpin, b->offset, 0);

    /* see if we need to decode this part before processing it */
    if (b->encoding == ENCBASE64
	|| b->encoding == ENCQUOTEDPRINTABLE
	|| b->encoding == ENCUUENCODED || plaintext
	|| mutt_is_text_part(b))    /* text subtypes may require character set
				     * conversion even with 8bit encoding. */
    {
	origType = b->type;

	if (!plaintext)
	{
	    CFMutableDataRef tempdata = CFDataCreateMutable(NULL, 0);
	    fp = s->fpout;
	    s->fpout = fwrapdata(tempdata);
	    CFRelease(tempdata);

	    /* decoding the attachment changes the size and offset, so save a copy
	    * of the "real" values now, and restore them after processing */
	    tmplength = b->length;
	    tmpoffset = b->offset;

	    decode = 1;
	}
	else
	    b->type = TYPETEXT;

	mutt_decode_attachment(b, s);

	if (decode)
	{
	    b->length = ftello(s->fpout);
	    b->offset = 0;
	    fp2 = s->fpout;
	    fseek(fp2, 0, SEEK_SET);

	    /* restore final destination & substitute the tempfile for input */
	    s->fpout = fp;
	    fp = s->fpin;
	    s->fpin = fp2;
	}

	b->type = origType;
    }

    /* process the (decoded) body part */
    if (handler)
    {
	handler(b, s);

	if (decode)
	{
	    b->length = tmplength;
	    b->offset = tmpoffset;

	    /* restore the original source stream */
	    safe_fclose (&s->fpin);
	    s->fpin = fp;
	}
    } else if (b->filename) {
	// no handler for part, so just include its filename
	fputc(' ', s->fpout);
	fputs(b->filename, s->fpout);
	fputc('\n', s->fpout);
    }
}

static void body_handler(BODY *b, STATE *s)
{
    int plaintext = 0;
    handler_t handler = NULL;

    if (b->type == TYPETEXT)
    {
	if (ascii_strcasecmp ("plain", b->subtype) == 0)
	{
	    if (!((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp(b)))
		handler = text_plain_handler;
	}
	else if (ascii_strcasecmp("enriched", b->subtype) == 0)
	    handler = text_html_handler;
	else if (ascii_strcasecmp("html", b->subtype) == 0)
	    handler = text_html_handler;
	else
	    plaintext = 1;
    }
    else if (b->type == TYPEMESSAGE)
    {
	if(mutt_is_message_type(b->type, b->subtype))
	    handler = message_handler;
	else if (!ascii_strcasecmp("delivery-status", b->subtype))
	    plaintext = 1;
	else if (!ascii_strcasecmp("external-body", b->subtype))
	    handler = external_body_handler;
    }
    else if (b->type == TYPEMULTIPART)
    {
	if (ascii_strcasecmp("alternative", b->subtype) == 0)
	    handler = alternative_handler;
	else if (WithCrypto && ascii_strcasecmp("signed", b->subtype) == 0)
	    handler = signed_handler;

	if (!handler)
	    handler = multipart_handler;

	if (b->encoding != ENC7BIT
	    && b->encoding != ENC8BIT
	    && b->encoding != ENCBINARY)
	{
	    b->encoding = ENC7BIT;
	}
    }
    else if (WithCrypto && b->type == TYPEAPPLICATION)
    {
	plaintext = !ascii_strcasecmp("pgp-keys", b->subtype);
    }

    run_decode_and_handler(b, s, handler, plaintext);
}

void free_message_header(void)
{
    free_message(Context);
}

CFMutableDataRef mutt_message_text(char *msgpath, void **hdr)
{
    char *path, *msgname, *subdir;
    int msgno;
    MESSAGE *msg;
    STATE s;
    CFMutableDataRef data;

    msgname = safe_strdup(basename(msgpath));
    path = safe_strdup(dirname(msgpath));
    subdir = safe_strdup(chomp_maildir_ext(path));

    set_context_path(Context, path);

    msgno = read_message(Context, subdir, msgname);
    if (msgno < 0) {
	fprintf(stderr, "failed to read message!\n");
	exit(-2);
    }

    if ((msg = mx_open_message(Context, msgno)) == NULL)
	return NULL;

    data = CFDataCreateMutable(NULL, 0);

    /* now make a text/plain version of the message */
    memset(&s, 0, sizeof(STATE));
    s.fpin = msg->fp;
    s.fpout = fwrapdata(data);
    s.flags = MUTT_DISPLAY | MUTT_CHARCONV;
    
    body_handler(Context->hdrs[msgno]->content, &s);
    safe_fclose(&s.fpout);

    mx_close_message(Context, &msg);

    if (hdr == NULL)
	free_message_header();
    else
	*hdr = (void *)Context->hdrs[msgno];

    FREE(&msgname);
    FREE(&path);
    FREE(&subdir);

    return data;
}


/** Test stubs */

#if TESTING
void show_data(CFMutableDataRef data)
{
    const UInt8 *contents = CFDataGetBytePtr(data);
    CFIndex len = CFDataGetLength(data);

    fwrite(contents, len, 1, stdout);
}

int main(int argc, char **argv, char **environ)
{
    dispatch_queue_t dqueue =
	dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_semaphore_t sync_sem;
    int filedes[2];
    FILE *f;
    struct mutt_to_html_args args;
    CFMutableDataRef data;
    int tflag = 0;
    char ch;

    while ((ch = getopt(argc, argv, "th?")) != -1) {
	switch (ch) {
	    case 't':
		tflag = 1;
		break;

	    case ':':
		printf("path=%s\n", optarg);
		break;

	    case '?':
	    case 'h':
	    default:
		printf("usage: %s [OPTION] path\n", basename(argv[0]));
		printf("\t-t\textract plain text (for indexing)\n");
		exit(0);
	}
    }
    argc -= optind;
    argv += optind;

    if (argc == 0) {
	fprintf(stderr, "no path given!\n");
	exit(1);
    } else if (argc > 1) {
	fprintf(stderr, "too many arguments!\n");
	exit(1);
    }

    init_mutt_iface(environ);

    if (tflag) {
	// show text for indexing
	data = mutt_message_text(argv[0], NULL);

	show_data(data);
	CFRelease(data);
    } else {
	// show html for previewing

	if (pipe(filedes) < 0) {
	    fprintf(stderr, "pipe() failed!\n");
	    exit(-3);
	}

	sync_sem = dispatch_semaphore_create(0);
	args.fdin = filedes[0];
	args.sync = sync_sem;
	args.body_class = "preview";
	dispatch_async_f(dqueue, &args, (dispatch_function_t)mutt_to_html);

	f = fdopen(filedes[1], "w");
	cat_mutt_message(argv[0], f);
	fclose(f);

	dispatch_semaphore_wait(sync_sem, DISPATCH_TIME_FOREVER);
	dispatch_release(sync_sem);

	if (args.dout) {
	    fputs((const char *)CFDataGetBytePtr(args.dout), stdout);
	    CFRelease(args.dout);
	}
    }

    free_mutt_iface();
    return 0;
}
#endif

