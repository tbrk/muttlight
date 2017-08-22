/*
 * Copyright (C) 2017 Timothy Bourke (tim@tbrk.org)
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

#ifndef _MUTTIFACE_H_
#define _MUTTIFACE_H_

#include <stdio.h>
#include <CoreFoundation/CFData.h>
#include <dispatch/dispatch.h>

void init_mutt_iface(char **environ);
void free_mutt_iface(void);

int cat_mutt_message(char *msgpath, FILE *fout);

struct mutt_to_html_args {
    int fdin;
    char *body_class;
    __unsafe_unretained dispatch_semaphore_t sync;
    CFDataRef dout;
};

void mutt_to_html(struct mutt_to_html_args *);

void free_message_header(void);

// if hdr != NULL, it is pointed at the HEADER of the parsed message
// and (in this case only) it is necessary to call free_message_header()
// when it is no longer in use (and before any subsequent calls to
// mutt_message_text).
CFMutableDataRef mutt_message_text(char *msgpath, void **hdr);

#endif
