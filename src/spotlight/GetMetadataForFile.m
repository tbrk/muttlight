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
#import <CoreData/CoreData.h>

#include "../muttiface.h"

#include <time.h>

#include "config.h"
#include "mutt.h"
#include "charset.h"
#include "rfc2047.h"

static NSString *str(const char *s)
{
    return [[NSString alloc] initWithCString: s
				    encoding: NSUTF8StringEncoding];
}

void add_addresses(ADDRESS *addr,
		   NSMutableArray *names,
		   NSMutableArray *addresses)
{
    if (addr && addr->mailbox) {
	NSString *mbox = str(addr->mailbox);
	[addresses addObject:mbox];

	if (addr->personal) {
	    [names addObject:str(addr->personal)];
	} else {
	    [names addObject:mbox];
	}

	add_addresses(addr->next, names, addresses);
    }
}

#define SETDATA(k, v) [(__bridge NSMutableDictionary *)data \
			    setObject: v forKey: (__bridge NSString *)k]
#define SETSDATA(k, v) [(__bridge NSMutableDictionary *)data \
			    setObject: v forKey: k]

#define MAILDIR_FLAG_MATCH @".*:2,(D?)(F?)(P?)(R?)(S?)(T?)$"
#define FLAG_MATCH_D 1
#define FLAG_MATCH_F 2
#define FLAG_MATCH_P 3
#define FLAG_MATCH_R 4
#define FLAG_MATCH_S 5
#define FLAG_MATCH_T 6

//  Extract useful information from email messages and set the values into
//  the attribute dictionary for Spotlight to include.

Boolean GetMetadataForFile(void *thisInterface,
	CFMutableDictionaryRef data,
	CFStringRef contentTypeUTI,
	CFStringRef pathToFile)
{
    Boolean ok = FALSE;

    if (!Charset) {
	//mutt_set_langinfo_charset();
	Charset = safe_strdup ("UTF-8");
    }
    
    @autoreleasepool {

    NSString *path = (__bridge NSString *)pathToFile;
    NSError *error = nil;

    NSRegularExpression *regex = [NSRegularExpression
	regularExpressionWithPattern:MAILDIR_FLAG_MATCH
			     options:NSRegularExpressionCaseInsensitive
			       error:&error];

    if ([(__bridge NSString *)contentTypeUTI
	    isEqualToString:@"org.tbrk.muttlight.email"])
    {
	HEADER *hdr;
	ENVELOPE *headers = NULL;
	NSMutableData *text;
	char *cpath = safe_strdup(
		[path cStringUsingEncoding: NSUTF8StringEncoding]);

	text = (__bridge NSMutableData *)
		    mutt_message_text(cpath, (void **)&hdr);
	FREE(&cpath);
	SETDATA(kMDItemTextContent,
		[[NSString alloc] initWithData:text
				      encoding:NSUTF8StringEncoding]);
	headers = hdr->env;

	if (headers != NULL) {
	    NSTextCheckingResult *match;
	    NSMutableArray *from_names = [[NSMutableArray alloc] init];
	    NSMutableArray *from_addrs = [[NSMutableArray alloc] init];
	    NSMutableArray *to_names   = [[NSMutableArray alloc] init];
	    NSMutableArray *to_addrs   = [[NSMutableArray alloc] init];

	    ok = TRUE;

	    SETDATA(kMDItemKind, @"Mail Message");
	    if (headers->message_id)
		SETDATA(kMDItemIdentifier, str(headers->message_id));

	    if (headers->subject) {
		NSString *subject;
		rfc2047_decode(&headers->subject);
		subject = str(headers->subject);
		SETDATA(kMDItemDisplayName, subject);
		SETDATA(kMDItemSubject, subject);
	    }

	    if (headers->date) {
		// TODO: keep the timezone information if possible
		time_t t_date = mutt_parse_date(headers->date, NULL);
		NSDate *date = [[NSDate alloc]
		    initWithTimeIntervalSince1970:(double)t_date];

		SETDATA(kMDItemContentCreationDate, date);
		SETSDATA(@"com_apple_mail_dateReceived", date);
	    }

	    add_addresses(headers->from, from_names, from_addrs);
	    add_addresses(headers->to, to_names, to_addrs);
	    add_addresses(headers->cc, to_names, to_addrs);
	    add_addresses(headers->bcc, to_names, to_addrs);

	    SETDATA(kMDItemAuthors, from_names);
	    SETDATA(kMDItemAuthorEmailAddresses, from_addrs);
	    SETDATA(kMDItemRecipients, to_names);
	    SETDATA(kMDItemRecipientEmailAddresses, to_addrs);

	    match = [regex firstMatchInString: path
				      options: 0
					range: NSMakeRange(0, [path length])];
	    if (match) {
		NSRange range_F = [match rangeAtIndex:FLAG_MATCH_F];
		NSRange range_S = [match rangeAtIndex:FLAG_MATCH_S];
		NSRange range_R = [match rangeAtIndex:FLAG_MATCH_R];

		SETSDATA(@"com_apple_mail_priority",
			 [NSNumber numberWithInteger: 3]);
		SETSDATA(@"com_apple_mail_flagged",
			 [NSNumber numberWithUnsignedInteger: range_F.length]);
		SETSDATA(@"com_apple_mail_read",
			 [NSNumber numberWithUnsignedInteger: range_S.length]);
		SETSDATA(@"com_apple_mail_repliedTo",
			 [NSNumber numberWithUnsignedInteger: range_R.length]);
	    }
	}

	free_message_header();
    } else
	NSLog(@"%@ not registered as org.tbrk.muttlight.email!", path);

    } // autorelease pool
    
    return ok;
}

