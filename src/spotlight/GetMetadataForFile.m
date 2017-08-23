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

// From: https://stackoverflow.com/a/30392716 (L.J. Wubbe)
static NSString *dataToUTF8String(NSData *data)
{
    // First try to do the 'standard' UTF-8 conversion
    NSString * bufferStr = [[NSString alloc] initWithData:data
                                                 encoding:NSUTF8StringEncoding];

    // if it fails, do the 'lossy' UTF8 conversion
    if (!bufferStr) {
        const Byte * buffer = [data bytes];

        NSMutableString * filteredString = [[NSMutableString alloc] init];

        int i = 0;
        while (i < [data length]) {

            int expectedLength = 1;

            if      ((buffer[i] & 0b10000000) == 0b00000000) expectedLength = 1;
            else if ((buffer[i] & 0b11100000) == 0b11000000) expectedLength = 2;
            else if ((buffer[i] & 0b11110000) == 0b11100000) expectedLength = 3;
            else if ((buffer[i] & 0b11111000) == 0b11110000) expectedLength = 4;
            else if ((buffer[i] & 0b11111100) == 0b11111000) expectedLength = 5;
            else if ((buffer[i] & 0b11111110) == 0b11111100) expectedLength = 6;

            int length = MIN(expectedLength, [data length] - i);
            NSData * character = [NSData dataWithBytes:&buffer[i] length:(sizeof(Byte) * length)];

            NSString * possibleString = [NSString stringWithUTF8String:[character bytes]];
            if (possibleString) {
                [filteredString appendString:possibleString];
            }
            i = i + expectedLength;
        }
        bufferStr = filteredString;
    }

    return bufferStr;
}

static int length_or_remaining(int length, const char* buf)
{
    int i;

    for (i=0; i < length; ++i) {
	if (!buf[i])
	    return i;
    }

    return length;
}

static NSString *str(const char *data)
{
    // First try to do the 'standard' UTF-8 conversion
    NSString *bufferStr = [NSString stringWithUTF8String: data];

    // if it fails, do the 'lossy' UTF8 conversion
    if (!bufferStr) {
        NSMutableString * filteredString = [[NSMutableString alloc] init];

        while (*data) {

            int expectedLength = 1;

            if      ((*data & 0b10000000) == 0b00000000) expectedLength = 1;
            else if ((*data & 0b11100000) == 0b11000000) expectedLength = 2;
            else if ((*data & 0b11110000) == 0b11100000) expectedLength = 3;
            else if ((*data & 0b11111000) == 0b11110000) expectedLength = 4;
            else if ((*data & 0b11111100) == 0b11111000) expectedLength = 5;
            else if ((*data & 0b11111110) == 0b11111100) expectedLength = 6;

            int length = length_or_remaining(expectedLength, data);
            NSData *character = [NSData dataWithBytes:data length:length];

            NSString *possibleString = [NSString
				    stringWithUTF8String:[character bytes]];
            if (possibleString) {
                [filteredString appendString:possibleString];
            }
	    data += expectedLength;
        }
        bufferStr = filteredString;
    }

    return bufferStr;
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

#define SETDATA(k, v)  [dict setObject: v forKey: (__bridge NSString *)k]
#define SETSDATA(k, v) [dict setObject: v forKey: k]

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

    NSMutableDictionary *dict = (__bridge NSMutableDictionary *)data;
    NSString *path = (__bridge NSString *)pathToFile;
    NSError *error = nil;

    NSRegularExpression *regex = [NSRegularExpression
	regularExpressionWithPattern:MAILDIR_FLAG_MATCH
			     options:NSRegularExpressionCaseInsensitive
			       error:&error];

    if ([(__bridge NSString *)contentTypeUTI
	    isEqualToString:@"org.tbrk.muttlight.email"])
    {
	HEADER *hdr = NULL;
	ENVELOPE *headers = NULL;
	NSMutableData *textdata = nil;
	char *cpath = safe_strdup(
		[path cStringUsingEncoding: NSUTF8StringEncoding]);

	SETDATA(kMDItemKind, @"Mail Message");

	textdata = (__bridge_transfer NSMutableData *)
		    mutt_message_text(cpath, (void **)&hdr);
	FREE(&cpath);

	SETDATA(kMDItemTextContent, dataToUTF8String(textdata));
	headers = hdr->env;

	if (headers != NULL) {
	    NSTextCheckingResult *match;
	    NSMutableArray *from_names = [[NSMutableArray alloc] init];
	    NSMutableArray *from_addrs = [[NSMutableArray alloc] init];
	    NSMutableArray *to_names   = [[NSMutableArray alloc] init];
	    NSMutableArray *to_addrs   = [[NSMutableArray alloc] init];

	    ok = TRUE;

	    if (headers->message_id)
		SETDATA(kMDItemIdentifier, str(headers->message_id));

	    if (headers->subject) {
		NSString *subject = NULL;
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

