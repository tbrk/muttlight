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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <sys/syslimits.h>

#include "../muttiface.h"

extern CFDictionaryRef email_css;

OSStatus
GeneratePreviewForURL(void *thisInterface,
                      QLPreviewRequestRef preview,
                      CFURLRef url,
                      CFStringRef contentTypeUTI,
                      CFDictionaryRef options)
{
    dispatch_queue_t dqueue =
	dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    int filedes[2];
    struct mutt_to_html_args args;
    FILE *f;
    char path[PATH_MAX];
    
    CFDictionaryRef properties = NULL;
    CFStringRef keys[] = {
        kQLPreviewPropertyTextEncodingNameKey,
        kQLPreviewPropertyMIMETypeKey,
        kQLPreviewPropertyAttachmentsKey,
    };
    void *values[sizeof(keys)];
    
    CFMutableDictionaryRef attachments;

    if (!CFURLGetFileSystemRepresentation(url, true,
		(UInt8 *)path, sizeof(path)))
        return noErr;

    if (pipe(filedes) < 0) return noErr;
    args.fdin = filedes[0];
    args.sync = dispatch_semaphore_create(0);
    args.body_class = "preview";

    dispatch_async_f(dqueue, &args, (dispatch_function_t)mutt_to_html);
    
    f = fdopen(filedes[1], "w");
    cat_mutt_message(path, f);
    fclose(f);

    dispatch_semaphore_wait(args.sync, DISPATCH_TIME_FOREVER);
    dispatch_release(args.sync);

    attachments = CFDictionaryCreateMutable(NULL, 0,
	    &kCFCopyStringDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(attachments, CFSTR("email.css"), email_css);
    
    values[0] = (void *)CFSTR("UTF-8");
    values[1] = (void *)CFSTR("text/html");
    values[2] = (void *)attachments;
    
    properties = CFDictionaryCreate(NULL,
	    (const void **)keys, (const void **)values,
	    sizeof(keys) / sizeof(void *),
	    &kCFCopyStringDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);

    QLPreviewRequestSetDataRepresentation(
	    preview, args.dout, kUTTypeHTML, properties);
    
    CFRelease(attachments);
    CFRelease(properties);
    CFRelease(args.dout);

    return noErr;
}

void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview)
{
}

