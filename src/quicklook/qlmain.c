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
#include <CoreFoundation/CFPlugInCOM.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#include "../muttiface.h"

/* ** globals */

CFDictionaryRef email_css;

/* ** constants */

// Don't modify this line
#define PLUGIN_ID "3B173283-F71D-436A-969A-2656D69B7A9A"

/* ** typedefs */

OSStatus GenerateThumbnailForURL(void *thisInterface,
                                 QLThumbnailRequestRef thumbnail,
                                 CFURLRef url,
                                 CFStringRef contentTypeUTI,
                                 CFDictionaryRef options,
                                 CGSize maxSize);

void CancelThumbnailGeneration(void* thisInterface,
                               QLThumbnailRequestRef thumbnail);

OSStatus GeneratePreviewForURL(void *thisInterface,
                               QLPreviewRequestRef preview,
                               CFURLRef url,
                               CFStringRef contentTypeUTI,
                               CFDictionaryRef options);

void CancelPreviewGeneration(void *thisInterface,
                             QLPreviewRequestRef preview);

/* The layout for an instance of QuickLookGeneratorPlugIn */
typedef struct __QuickLookGeneratorPluginType
{
    void        *conduitInterface;
    CFUUIDRef    factoryID;
    UInt32       refCount;
} QuickLookGeneratorPluginType;


/* ** prototypes */
// Forward declaration for the IUnknown implementation.

QuickLookGeneratorPluginType *
AllocQuickLookGeneratorPluginType(CFUUIDRef inFactoryID);

void
DeallocQuickLookGeneratorPluginType(QuickLookGeneratorPluginType *thisInstance);

HRESULT
QuickLookGeneratorQueryInterface(void *thisInstance, REFIID iid, LPVOID *ppv);

void *
QuickLookGeneratorPluginFactory(CFAllocatorRef allocator, CFUUIDRef typeID);

ULONG QuickLookGeneratorPluginAddRef(void *thisInstance);
ULONG QuickLookGeneratorPluginRelease(void *thisInstance);

/* ** Utility functions */
static CFDataRef data_from_file(CFURLRef path)
{
    CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, path);
    CFIndex read;
    CFMutableDataRef data = NULL;
    UInt8 buffer[4096];
    
    if (!stream || !CFReadStreamOpen(stream)) return NULL;
    
    read = CFReadStreamRead(stream, buffer, sizeof(buffer));
    if (read > 0)
	data = CFDataCreateMutable(NULL, 0);
    while (read > 0) {
        CFDataAppendBytes(data, buffer, read);
        read = CFReadStreamRead(stream, buffer, sizeof(buffer));
    }
    
    CFReadStreamClose(stream);
    CFRelease(stream);
    return data;
}

/*  The QLGeneratorInterfaceStruct function table. */

static QLGeneratorInterfaceStruct myInterfaceFtbl = {
    NULL,
    QuickLookGeneratorQueryInterface,
    QuickLookGeneratorPluginAddRef,
    QuickLookGeneratorPluginRelease,
    NULL,
    NULL,
    NULL,
    NULL
};

/* Utility function that allocates a new instance.
 You can do some initial setup for the generator here if you wish
 like allocating globals etc... */

QuickLookGeneratorPluginType *
AllocQuickLookGeneratorPluginType(CFUUIDRef inFactoryID)
{
    QuickLookGeneratorPluginType *theNewInstance;
    CFStringRef keys[] = {
        kQLPreviewPropertyMIMETypeKey,
        kQLPreviewPropertyAttachmentDataKey,
    };
    void *values[sizeof(keys)];
    CFStringRef email_css_path;
    
    theNewInstance = (QuickLookGeneratorPluginType *)malloc(
	    sizeof(QuickLookGeneratorPluginType));
    memset(theNewInstance, 0, sizeof(QuickLookGeneratorPluginType));
    
    /* Point to the function table Malloc enough to store the stuff and copy
     * the filler from myInterfaceFtbl over */
    theNewInstance->conduitInterface =
    malloc(sizeof(QLGeneratorInterfaceStruct));
    memcpy(theNewInstance->conduitInterface,
           &myInterfaceFtbl,
           sizeof(QLGeneratorInterfaceStruct));
    
    /*  Retain and keep an open instance refcount for each factory. */
    theNewInstance->factoryID = CFRetain(inFactoryID);
    CFPlugInAddInstanceForFactory(inFactoryID);
    
    /* Initialize global structures */
    // TODO: pass address to config file
    // Set path with: mutt_str_replace (&Muttrc, optarg);
    // Ignore system rc file: flags |= MUTT_NOSYSRC;
    //			     (pass to mutt_init)
    init_mutt_iface(NULL);
    
    values[0] = (void *)CFSTR("text/css");
    values[1] = NULL;
    
    email_css_path = (CFStringRef)CFPreferencesCopyAppValue(
	    CFSTR("emailStyleSheet"), kCFPreferencesCurrentApplication);
    if (!email_css_path)
        email_css_path = CFSTR("email.css");

    if (CFStringHasPrefix(email_css_path, CFSTR("file://"))
	|| CFStringHasPrefix(email_css_path, CFSTR("/")))
    {
	CFURLRef email_css_url =
	    CFURLCreateWithString(NULL, email_css_path, NULL);
        values[1] = (void *)data_from_file(email_css_url);
        CFRelease(email_css_url);
    } else {
	CFBundleRef bundle;
        CFURLRef bundle_url, resource_url, bundle_resource_url, email_css_url;

	bundle = CFBundleGetBundleWithIdentifier(
		    CFSTR("org.tbrk.muttlight.quicklook"));
        bundle_url = CFBundleCopyBundleURL(bundle);
	resource_url = CFBundleCopyResourcesDirectoryURL(bundle);
	bundle_resource_url = CFURLCreateCopyAppendingPathComponent(NULL,
		bundle_url, CFURLGetString(resource_url), 0);
	email_css_url = CFURLCreateCopyAppendingPathComponent(NULL,
		bundle_resource_url, email_css_path, 0);

        values[1] = (void *)data_from_file(email_css_url);

        CFRelease(email_css_url);
        CFRelease(bundle_resource_url);
        CFRelease(resource_url);
        CFRelease(bundle_url);
        CFRelease(bundle);
    }

    CFRelease(email_css_path);

    email_css = CFDictionaryCreate(NULL,
	    (const void **)keys, (const void **)values,
	    sizeof(keys) / sizeof(void *),
	    &kCFCopyStringDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
    CFRelease(values[1]);
    
    /* This function returns the IUnknown interface so set refCount to one. */
    theNewInstance->refCount = 1;
    return theNewInstance;
}

/* Utility function that deallocates the instance when
 the refCount goes to zero.
 In the current implementation generator interfaces are never deallocated
 but implement this as this might change in the future */

void DeallocQuickLookGeneratorPluginType(
	QuickLookGeneratorPluginType *thisInstance)
{
    CFUUIDRef theFactoryID;
    
    theFactoryID = thisInstance->factoryID;
    /* Free the conduitInterface table up */
    free(thisInstance->conduitInterface);
    
    /* Free global structures */
    free_mutt_iface();
    CFRelease(email_css);
    
    /* Free the instance structure */
    free(thisInstance);
    if (theFactoryID){
        CFPlugInRemoveInstanceForFactory(theFactoryID);
        CFRelease(theFactoryID);
    }
}

/* Implementation of the IUnknown QueryInterface function. */

HRESULT QuickLookGeneratorQueryInterface(
	void *thisInstance, REFIID iid, LPVOID *ppv)
{
    CFUUIDRef interfaceID;
    
    interfaceID = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault,iid);
    
    if (CFEqual(interfaceID,kQLGeneratorCallbacksInterfaceID)){
        /* If the Right interface was requested, bump the ref count,
         * set the ppv parameter equal to the instance, and
         * return good status.
         */
        ((QLGeneratorInterfaceStruct *)
         ((QuickLookGeneratorPluginType *)thisInstance)->conduitInterface)
        ->GenerateThumbnailForURL = GenerateThumbnailForURL;
        
        ((QLGeneratorInterfaceStruct *)
         ((QuickLookGeneratorPluginType *)thisInstance)->conduitInterface)
        ->CancelThumbnailGeneration = CancelThumbnailGeneration;
        
        ((QLGeneratorInterfaceStruct *)
         ((QuickLookGeneratorPluginType *)thisInstance)->conduitInterface)
        ->GeneratePreviewForURL = GeneratePreviewForURL;
        
        ((QLGeneratorInterfaceStruct *)
         ((QuickLookGeneratorPluginType *)thisInstance)->conduitInterface)
        ->CancelPreviewGeneration = CancelPreviewGeneration;
        
        ((QLGeneratorInterfaceStruct *)
         ((QuickLookGeneratorPluginType*)thisInstance)->conduitInterface)
        ->AddRef(thisInstance);
        
        *ppv = thisInstance;
        CFRelease(interfaceID);
        return S_OK;
    } else {
        /* Requested interface unknown, bail with error. */
        *ppv = NULL;
        CFRelease(interfaceID);
        return E_NOINTERFACE;
    }
}

/* Implementation of reference counting for this type. Whenever an interface
 is requested, bump the refCount for the instance. NOTE: returning the
 refcount is a convention but is not required so don't rely on it. */

ULONG QuickLookGeneratorPluginAddRef(void *thisInstance)
{
    ((QuickLookGeneratorPluginType *)thisInstance )->refCount += 1;
    return ((QuickLookGeneratorPluginType*) thisInstance)->refCount;
}

/* When an interface is released, decrement the refCount.
 If the refCount goes to zero, deallocate the instance. */

ULONG QuickLookGeneratorPluginRelease(void *thisInstance)
{
    ((QuickLookGeneratorPluginType*)thisInstance)->refCount -= 1;
    if (((QuickLookGeneratorPluginType*)thisInstance)->refCount == 0) {
        DeallocQuickLookGeneratorPluginType(
	    (QuickLookGeneratorPluginType*)thisInstance);
        return 0;
    }else{
        return ((QuickLookGeneratorPluginType*) thisInstance )->refCount;
    }
}

void *QuickLookGeneratorPluginFactory(CFAllocatorRef allocator,
                                      CFUUIDRef typeID)
{
    QuickLookGeneratorPluginType *result;
    CFUUIDRef                 uuid;
    
    /* If correct type is being requested, allocate an
     * instance of kQLGeneratorTypeID and return the IUnknown interface. */
    if (CFEqual(typeID,kQLGeneratorTypeID)) {
        uuid = CFUUIDCreateFromString(kCFAllocatorDefault,CFSTR(PLUGIN_ID));
        result = AllocQuickLookGeneratorPluginType(uuid);
        CFRelease(uuid);
        return result;
    }
    
    /* If the requested type is incorrect, return NULL. */
    return NULL;
}

