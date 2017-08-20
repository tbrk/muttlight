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

#include "../muttiface.h"

//  constants

#define PLUGIN_ID "81CC65EE-B006-490C-8CB8-3541E36A5DCF"

//  typedefs

// The import function to be implemented in GetMetadataForFile.c
Boolean GetMetadataForFile(void *thisInterface, 
			   CFMutableDictionaryRef attributes, 
			   CFStringRef contentTypeUTI,
			   CFStringRef pathToFile);
               
// The layout for an instance of MetaDataImporterPlugIn 
typedef struct __MetadataImporterPluginType
{
    MDImporterInterfaceStruct *conduitInterface;
    CFUUIDRef                 factoryID;
    UInt32                    refCount;
} MetadataImporterPluginType;

//  prototypes

MetadataImporterPluginType *
AllocMetadataImporterPluginType(CFUUIDRef inFactoryID);

void
DeallocMetadataImporterPluginType(MetadataImporterPluginType *thisInstance);

HRESULT
MetadataImporterQueryInterface(void *thisInstance,REFIID iid,LPVOID *ppv);

void *
MetadataImporterPluginFactory(CFAllocatorRef allocator,CFUUIDRef typeID);

ULONG
MetadataImporterPluginAddRef(void *thisInstance);

ULONG
MetadataImporterPluginRelease(void *thisInstance);

//  The TestInterface function table.
static MDImporterInterfaceStruct testInterfaceFtbl = {
    NULL,
    MetadataImporterQueryInterface,
    MetadataImporterPluginAddRef,
    MetadataImporterPluginRelease,
    GetMetadataForFile
};


// Utility function that allocates a new instance.
// You can do some initial setup for the importer here if you wish
// like allocating globals etc...
MetadataImporterPluginType *
AllocMetadataImporterPluginType(CFUUIDRef inFactoryID)
{
    MetadataImporterPluginType *theNewInstance;

    theNewInstance = (MetadataImporterPluginType *)
	malloc(sizeof(MetadataImporterPluginType));
    memset(theNewInstance,0,sizeof(MetadataImporterPluginType));

    /* Point to the function table */
    theNewInstance->conduitInterface = &testInterfaceFtbl;

    /*  Retain and keep an open instance refcount for each factory. */
    theNewInstance->factoryID = CFRetain(inFactoryID);
    CFPlugInAddInstanceForFactory(inFactoryID);

    /* Initialize global structures */
    init_mutt_iface(NULL);

    /* Returns the IUnknown interface so set the refCount to one. */
    theNewInstance->refCount = 1;
    return theNewInstance;
}

// Utility function that deallocates the instance when
// the refCount goes to zero.
// In the current implementation importer interfaces are never deallocated
// but implement this as this might change in the future
void
DeallocMetadataImporterPluginType(MetadataImporterPluginType *thisInstance)
{
    CFUUIDRef theFactoryID;

    theFactoryID = thisInstance->factoryID;
    free(thisInstance);
    if (theFactoryID){
        CFPlugInRemoveInstanceForFactory(theFactoryID);
        CFRelease(theFactoryID);
    }
}

// Implementation of the IUnknown QueryInterface function.
HRESULT
MetadataImporterQueryInterface(void *thisInstance,REFIID iid,LPVOID *ppv)
{
    CFUUIDRef interfaceID;

    interfaceID = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault,iid);

    if (CFEqual(interfaceID,kMDImporterInterfaceID)){
	/* If the Right interface was requested, bump the ref count,
	 * set the ppv parameter equal to the instance, and
	 * return good status. */
        ((MetadataImporterPluginType*)thisInstance)
	    ->conduitInterface->AddRef(thisInstance);
        *ppv = thisInstance;
        CFRelease(interfaceID);
        return S_OK;
    }else{
        if (CFEqual(interfaceID,IUnknownUUID)){
	    /* If the IUnknown interface was requested, same as above. */
            ((MetadataImporterPluginType*)thisInstance)
		->conduitInterface->AddRef(thisInstance);
            *ppv = thisInstance;
            CFRelease(interfaceID);
            return S_OK;
        }else{
	    /* Requested interface unknown, bail with error. */
            *ppv = NULL;
            CFRelease(interfaceID);
            return E_NOINTERFACE;
        }
    }
}

// Implementation of reference counting for this type. Whenever an interface
// is requested, bump the refCount for the instance. NOTE: returning the
// refcount is a convention but is not required so don't rely on it.
ULONG MetadataImporterPluginAddRef(void *thisInstance)
{
    ((MetadataImporterPluginType *)thisInstance )->refCount += 1;
    return ((MetadataImporterPluginType*) thisInstance)->refCount;
}

// When an interface is released, decrement the refCount.
// If the refCount goes to zero, deallocate the instance.
ULONG MetadataImporterPluginRelease(void *thisInstance)
{
    ((MetadataImporterPluginType*)thisInstance)->refCount -= 1;
    if (((MetadataImporterPluginType*)thisInstance)->refCount == 0){
        DeallocMetadataImporterPluginType(
		(MetadataImporterPluginType*)thisInstance);
        return 0;
    }else{
        return ((MetadataImporterPluginType*) thisInstance )->refCount;
    }
}

// Implementation of the factory function for this type.
void *MetadataImporterPluginFactory(CFAllocatorRef allocator,CFUUIDRef typeID)
{
    MetadataImporterPluginType *result;
    CFUUIDRef                 uuid;

    /* If correct type is being requested, allocate an
     * instance of TestType and return the IUnknown interface. */
    if (CFEqual(typeID,kMDImporterTypeID)){
        uuid = CFUUIDCreateFromString(kCFAllocatorDefault, CFSTR(PLUGIN_ID));
        result = AllocMetadataImporterPluginType(uuid);
        CFRelease(uuid);
        return result;
    }
    /* If the requested type is incorrect, return NULL. */
    return NULL;
}

