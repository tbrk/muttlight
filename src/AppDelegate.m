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

#import "AppDelegate.h"

static const __unsafe_unretained NSString * allflags[]
    = { @"d", @"f", @"p", @"r", @"s", @"t", 0 };

void generate(const __unsafe_unretained NSString **flags,
              NSString *prefix,
              void (^f)(NSString *))
{
    if (*flags != 0) {
        NSString *prefix2 = [prefix stringByAppendingString:(NSString *)*flags];
        generate(flags + 1, prefix, f);
        f(prefix2);
        generate(flags + 1, prefix2, f);
    }
}

NSArray * extensionsWithFlags(NSDictionary *extensions)
{
    NSInteger numEnabledExtensions = 0;
    for (NSString *ext in extensions.keyEnumerator) {
        if ([extensions[ext] isEqualTo: @1])
            numEnabledExtensions++;
    }

    NSMutableArray *result = [NSMutableArray
                              arrayWithCapacity: numEnabledExtensions * 64];

    __block NSInteger i = 0;
    for (NSString *ext in extensions.keyEnumerator) {
        if ([extensions[ext] isEqualTo: @1]) {
            NSString *extWith2 = [ext stringByAppendingString: @":2,"];
            result[i++] = extWith2;
            generate(allflags, extWith2, ^(NSString *s) { result[i++] = s; });
        }
    }

    return result;
}

BOOL updatePlistFilenameExtensions(NSURL *plistPath, NSArray *extensions)
{
    BOOL updated = false;

    NSMutableDictionary *plist
        = [NSMutableDictionary dictionaryWithContentsOfURL: plistPath];

    NSString *typeDeclarationsKey = (NSString *)
        ([plist objectForKey: (NSString *)kUTExportedTypeDeclarationsKey]
            ? kUTExportedTypeDeclarationsKey
            : kUTImportedTypeDeclarationsKey);

    NSMutableArray *typeDeclarations = plist[typeDeclarationsKey];
    if (typeDeclarations == nil) return false;

    for (NSMutableDictionary *typeDeclaration in typeDeclarations) {
        if ([typeDeclaration[(NSString *)kUTTypeIdentifierKey]
             isEqualTo: MUTTLIGHT_FILETYPE_KEY])
        {
            [typeDeclaration[(NSString *)kUTTypeTagSpecificationKey]
                setObject:extensions
                   forKey:@"public.filename-extension"];
            updated = true;
        }
    }

    if (!updated) return false;

    plist[typeDeclarationsKey] = typeDeclarations;

    return [plist writeToURL: plistPath atomically: true];
}

/** AppDelegate */

@interface AppDelegate()

@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSArrayController *extensionsArrayController;

@property (retain) NSMetadataQuery *query;
@property (retain) NSMutableSet *alreadyFound;

@end

@implementation AppDelegate

- (id)init {

    if ((self = [super init])) {
        NSError *error = nil;

        self.hasDovecot = NO;
        self.extensions = [NSMutableArray array];

        self.regex_maildir =
            [NSRegularExpression
                regularExpressionWithPattern:@"\\.([^.]*):2,D?F?P?R?S?T?$"
                                     options:0
                                       error:&error];
        
        self.regex_dovecot =
            [NSRegularExpression
                regularExpressionWithPattern:@".*,S=\\d+,W=\\d+$"
                                     options:0
                                       error:&error];

	// Register the internalized app with LaunchServices
	NSBundle *bundle = [NSBundle bundleWithIdentifier: MUTTLIGHT_KEY];
	NSURL *bundle_url = [bundle bundleURL];
	NSURL *launcher_url = [NSURL
		URLWithString: @"Contents/MacOS/Muttlight Launcher.app"
		relativeToURL:bundle_url];
	LSRegisterURL((__bridge CFURLRef)launcher_url, false);
    }

    return self;
}

- (NSArray *)sortByExtension {
    return [NSArray arrayWithObject:
                [NSSortDescriptor sortDescriptorWithKey:@"extension"
                                              ascending:YES]];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSDictionary *exts = [prefs dictionaryForKey: @"extensions"];

    if (exts == nil) {
	exts = @{ @"mbox" : @YES };
	[prefs setObject:exts forKey:@"extensions"];
    }

    [self dictToExtensions: exts];
    [self startSearching];
}

- (IBAction)revertPressed:(id)sender {
    [self stopSearching:nil];

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    [self dictToExtensions:
        [prefs dictionaryForKey: @"extensions"]];
}

- (IBAction)applyPressed:(id)sender {
    NSDictionary *extensionsDict = [self extensionsToDict];

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    [prefs setObject:extensionsDict forKey:@"extensions"];

    // write selected extensions into plist files.

    NSBundle *bundle = [NSBundle bundleWithIdentifier: MUTTLIGHT_KEY];
    NSURL *bundle_url = [bundle bundleURL];
    NSArray *plistURLs = @[
        [NSURL URLWithString: @"Contents/Info.plist"
               relativeToURL:bundle_url],
        [NSURL URLWithString:
                @"Contents/Library/QuickLook/muttlight-quicklook.qlgenerator"
               relativeToURL:bundle_url],
        [NSURL URLWithString:
         @"Contents/Library/Spot/muttlight-spotlight.mdimporter"
               relativeToURL:bundle_url],
    ];

    NSArray *exts = extensionsWithFlags(extensionsDict);
    for (NSURL *url in plistURLs) {
        updatePlistFilenameExtensions(url, exts);
    }

    // Reregister with LaunchServices (touch; lsregister)
    NSFileManager *filemgr = [NSFileManager defaultManager];
    NSURL *exe_url = [NSURL
	    URLWithString: @"Contents/MacOS/Muttlight"
	    relativeToURL:bundle_url];
    [filemgr setAttributes: [NSDictionary
				dictionaryWithObject: [NSDate date]
					      forKey: NSFileModificationDate]
	      ofItemAtPath: [exe_url path]
		     error: nil];
    LSRegisterURL((__bridge CFURLRef)bundle_url, true);

    [[NSApplication sharedApplication] terminate:nil];
}

- (NSDictionary *)extensionsToDict {
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    
    for (NSDictionary *element in self.extensions) {
        NSString *k = element[@"extension"];
        id v = element[@"enabled"];
        
        if (k != nil)
            result[k] = (result[k]
                         || v == nil
                         || [v isEqualTo:@YES]
                         ? @YES : @NO);
    }
    
    return result;
}

- (void)dictToExtensions:(NSDictionary *)dict {
    [self.extensionsArrayController removeObjects:
     self.extensionsArrayController.arrangedObjects];
    
    for (NSString *k in [dict keyEnumerator]) {
        [self.extensionsArrayController
         addObject: [NSMutableDictionary
                     dictionaryWithObjectsAndKeys:
                     k, @"extension",
                     ([dict[k] isEqualTo: @1]? @YES : @NO), @"enabled",
                     nil]];
    }
}

- (void)startSearching {
    
    self.alreadyFound = [NSMutableSet new];
    self.query = [[NSMetadataQuery alloc] init];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(stopSearching:)
               name:NSMetadataQueryDidFinishGatheringNotification
             object:self.query];

    self.query.delegate = self;

    [self.query setPredicate:[NSPredicate
                    predicateWithFormat:@"kMDItemFSName like[c] '*.*:2,*'"]];

    [self.query setSearchScopes:
        [NSArray arrayWithObjects: NSMetadataQueryUserHomeScope,
                                   /* NSMetadataQueryUbiquitousDocumentsScope, */
                                   nil]];

    if ([self.query startQuery])
        self.isSearching = YES;
}

- (id)metadataQuery:(NSMetadataQuery *)query
         replacementObjectForResultObject:(NSMetadataItem *)item
{
    NSString *result = nil;
    NSString *name = [item
                      valueForAttribute:(__bridge NSString *)kMDItemFSName];

    NSTextCheckingResult *match = [self.regex_maildir
                                   firstMatchInString: name
                                   options: 0
                                   range: NSMakeRange(0, [name length])];
    
    if (match) {
        result = [name substringWithRange:[match rangeAtIndex:1]];
        
        if ([self.regex_dovecot
             numberOfMatchesInString: result
             options: 0
             range: NSMakeRange(0, [result length])] > 0)
        {
            self.hasDovecot = YES;

        } else if (![self.alreadyFound member: result]) {
            [[NSOperationQueue mainQueue] addOperationWithBlock:^{
                [self newResult: result];
            }];
            [self.alreadyFound addObject: result];
        }
    }
    
    return @"";
}

- (void)newResult:(NSString *)result {
    if ([self.extensions
            indexOfObjectPassingTest:^BOOL(id _Nonnull obj,
                                           NSUInteger idx,
                                           BOOL * _Nonnull stop) {
                NSDictionary *entry = (NSDictionary *)obj;
                return [((NSString *)entry[@"extension"])
                            isEqualToString:result];
            }] == NSNotFound)
    {
        [self.extensionsArrayController
            addObject:[NSMutableDictionary dictionaryWithDictionary:
                       @{ @"extension" : result, @"enabled" : @NO }]];
    };
}

- (void)stopSearching:(id)sender
{
    [self.query stopQuery];
    
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSMetadataQueryDidFinishGatheringNotification
                object:self.query];

    // When the Query is removed the query results are also lost.
    self.query = nil;
    self.isSearching = NO;
}

- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename
{
    return NO;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}

@end
