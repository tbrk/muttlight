Muttlight design notes
======================

[2013 mails on Mutt and OS X](http://thread.gmane.org/gmane.mail.mutt.devel/20662)

MailDir
-------

There are [six flags](http://cr.yp.to/proto/maildir.html):

* `D`: draft
* `F`: flagged
* `P`: passed (resent/forwarded/bounced)
* `R`: replied
* `S`: seen
* `T`: trashed

Flags must be stored in ASCII order.

The leading `2,` indicates version 2 (production) of the standard.

Filenames:
```
.mbox:2,S
.<hostname>:2,S
```

There are thus 64 possible combinations (per hostname).

```ocaml
(* Generate all possible combinations in OCaml. *)

let rec generate' xs =
  match xs with
  | [] -> []
  | x::xs ->
    let xs' = generate' xs in
    x :: List.map (fun s -> x ^ s) xs' @ xs'

let generate xs = "" :: generate' xs

let print_query s = Printf.printf "kMDItemFSName=\"*:2,%s\"||" s
(* let print_query = print_endline *)

let _ = List.iter print_query (generate ["D"; "F"; "P"; "R"; "S"; "T"])
let _ = print_endline "false"
```

Compile (`ocamlc -o gen gen.ml`), then run

```
mdfind `./gen`
```

[DoveCot](https://wiki2.dovecot.org/MailboxFormat/Maildir) uses an extension 
of the basic MailDir scheme. Filenames are of the form:

```
1035478339.27041_118.foo.org,S=1000,W=1030:2,S
```

where the `S=` field gives the file size, and the `W=` field gives the 
"rfc822" size. This extended scheme cannot be supported by our tool.

TODO: Provide a simple script to (safely) rename such files.

It may be easy to adapt the application to work with
[MH](https://en.wikipedia.org/wiki/MH_Message_Handling_System).

Spotlight
=========

[Notes on spotlight](http://osxnotes.net/spotlight.html)

Attributes

* `kMDItemFSName`: basename in file system
* `kMDItemDisplayName`: name used in result list
* `kMDItemFSCreationDate`: creation time

List all indexed mbox files:
```
mdfind 'kMDItemFSName=*.mbox:*'
```

See attributes of a file: `mdls <filename>`

[About File Metadata Queries](https://developer.apple.com/library/content/documentation/Carbon/Conceptual/SpotlightQuery/Concepts/Introduction.html)

[Spotlight Importers](https://developer.apple.com/library/content/documentation/Carbon/Conceptual/MDImporters/MDImporters.html#//apple_ref/doc/uid/TP40001267)

A spotlight importer is a small plug-in bundle.
Only one importer is allowed per _uniform type identifier (UTI)_.
Specify supported UTIs in the importers Info.plist file.

Xcode includes a Spotlight project template that provides the required 
CFPlugin support, as well as templates for the required schema file.

Include inside app bundle inside the `MyApp.app/Contents/Library/Spotlight` 
subdirectory.

```
MyApp.app/Contents/Library/Spotlight/SpotlightImporter.mdimporter
+--/Contents
   +--Info.plist
   +--MacOS/SpotlightImporter
   +--Resources/schema.strings      (custom attribute names)
   +--Resources/schema.xml          (treatment of attributes/custom metadata)
```

Testing:

* Show installed Spotlight plugins: `/usr/bin/mdimport -L`
* Testing importer: `/usr/bin/mdimport -d2 test.myCustomDocument`
* Explicitly register application: `lsregister -f MyApp.app`

Results of running `mdls` on a Mail draft:

```
_kMDItemOwnerUserID            = 501
com_apple_mail_dateLastViewed  = 2017-07-09 10:01:35 +0000
com_apple_mail_dateReceived    = 2017-07-09 10:01:35 +0000
com_apple_mail_flagged         = 0
com_apple_mail_messageID       = "GIHOFmmmhBFl"
com_apple_mail_priority        = 3
com_apple_mail_read            = 1
com_apple_mail_repliedTo       = 0
kMDItemAccountIdentifier       = "6E2A5D1B-7F22-426A-8D88-0963A36A3C08"
kMDItemAuthorEmailAddresses    = (
    "tim@tbrk.org"
)
kMDItemAuthors                 = (
    "tim@tbrk.org"
)
kMDItemContentCreationDate     = 2017-07-09 10:01:35 +0000
kMDItemContentModificationDate = 2017-07-09 10:01:35 +0000
kMDItemContentType             = "com.apple.mail.emlx"
kMDItemContentTypeTree         = (
    "com.apple.mail.emlx",
    "public.data",
    "public.item",
    "public.email-message",
    "public.message"
)
kMDItemDateAdded               = 2017-07-01 17:47:10 +0000
kMDItemDisplayName             = "Test email message 2"
kMDItemFSContentChangeDate     = 2017-07-09 10:01:35 +0000
kMDItemFSCreationDate          = 2017-07-01 17:47:10 +0000
kMDItemFSCreatorCode           = ""
kMDItemFSFinderFlags           = 0
kMDItemFSHasCustomIcon         = (null)
kMDItemFSInvisible             = 0
kMDItemFSIsExtensionHidden     = 0
kMDItemFSIsStationery          = (null)
kMDItemFSLabel                 = 0
kMDItemFSName                  = "143923.emlx"
kMDItemFSNodeCount             = (null)
kMDItemFSOwnerGroupID          = 20
kMDItemFSOwnerUserID           = 501
kMDItemFSSize                  = 1286
kMDItemFSTypeCode              = ""
kMDItemIdentifier              = "<8698CBE2-4ABF-464B-8F61-B11D93EF07AE@tbrk.org>"
kMDItemIsApplicationManaged    = 1
kMDItemIsExistingThread        = 0
kMDItemIsLikelyJunk            = 0
kMDItemKind                    = "Mail Message"
kMDItemLastUsedDate            = 2017-07-09 10:01:35 +0000
kMDItemLogicalSize             = 1286
kMDItemMailboxes               = (
    "mailbox.drafts"
)
kMDItemPhysicalSize            = 4096
kMDItemRecipientEmailAddresses = (
    "tim@tbrk.org",
    "jean@tbrk.org",
    "andre@tbrk.org",
)
kMDItemRecipients              = (
    "Timothy Bourke",
    "Jean Do",
    "Andre\U0301 Who"
)
kMDItemSubject                 = "Test email message 2"
kMDItemUseCount                = 4
kMDItemUsedDates               = (
    "2017-06-30 22:00:00 +0000",
    "2017-07-08 22:00:00 +0000"
)
```

Fill in information:

```
com_apple_mail_dateReceived    = from header Date: field.
com_apple_mail_flagged         = from filename (F)
com_apple_mail_read            = from filename (S)
com_apple_mail_repliedTo       = from filename (R)

kMDItemMailboxes               = ( "mailbox.drafts") ?

kMDItemKind                    = "Mail Message"
kMDItemIdentifier              = from header Message-Id: field
kMDItemDisplayName             = from header Subject: field
kMDItemSubject                 = from header Subject: field
kMDItemContentCreationDate     = from header Date: field.

kMDItemAuthorEmailAddresses    = ( from header From: field )    Array of CFStrings
kMDItemAuthors                 = ( from header From: field )    Array of CFStrings
kMDItemRecipientEmailAddresses = ( from header To: field )
kMDItemRecipients              = ( from header To: field )

kMDItemContentType             = "org.tbrk.muttlight.email"
kMDItemContentTypeTree         = (                            UTI hierarchy of file
    "com.apple.mail.emlx",
    "public.data",
    "public.item",
    "public.email-message",
    "public.message"
) ?
```

The size of the Spotlight index for a given volume can be determined by 
running `du -h -d 1 /.Spotlight-V100`.

Spotlight still indexes files when no plugin is registered for them—that is, 
the files appear in the search results for keywords that they contain. It 
seems just to treat their contents as plain text. When a plugin is 
registered, this does not happen automatically; it is necessary to return 
the `kMDItemTextContent` field. There are at least four possible ways to do 
this.

1. Recurse through the message (MIME) parts, including text directly, 
   turning HTML into text, and possibly calling other filters to treat 
   binary attachments. This is the ideal solution, but demands 
   non-negligible programming and debugging time.

2. Use the mutt pager to turn a message into plain text, as is already done 
   for the Quick Look plugin. This approach leverages the existing mutt code 
   and attachments can be treated using the mailcap mechanism. The 
   disadvantage is that many types of attachements (.pdf, .xls, .doc, 
   etcetera) are not normally displayed as plain text by mutt. 
   Unfortunately, this approach is not easy to implement without duplicating 
   much of the mutt source code, since Spotlight importers execute in a 
   sandbox which seems to prohibit them from creating temporary files and 
   directories. The calls to `mkdtemp` in the mutt pager thus fail and the 
   plugin crashes.

3. Simply slurp the whole file into `kMDItemTextContent`. This approach is 
   easy to implement and may be no worse than before installation of the 
   plugin. Besides not properly treating file attachments, it does not 
   decode RFC2045 text nor strip tags from HTML. It may cause spotlight to 
   generate a larger index file than necessary.

4. Use a hybrid of 1 and 3: recurse through the parts, using mutt functions 
   to decode RFC2045 text and `NSAttributedString` or custom code to decode 
   HTML, and ignoring other attachments. This seems to be a reasonable 
   compromise between development effort and results. It has been 
   implemented in the current version.

Quick Look
==========

There are several existing open-source "Quick Look Plugins":

* http://whomwah.github.io/qlstephen/
* https://github.com/sindresorhus/quick-look-plugins
* http://www.quicklookplugins.com

It does not seem possible to somehow reuse the existing Mail Quick Look 
feature.

Put Quick Look plugins in `/Library/Quick Look` and activate by resetting 
Quick Look with `qlmanage -r`.

Quick Look can be invoked with ⌘-Y or `qlmanage -p file`.

The following text summarizes the Apple
[Quick Look Programming](https://developer.apple.com/library/content/documentation/UserExperience/Conceptual/Quicklook_Programming_Guide/Introduction/Introduction.html#//apple_ref/doc/uid/TP40005020)
docs.

Quick Look displays

* thumbnail: static image depicting a document (multiple at once).
* preview: a larger representation of a document (one at a time).

Architecture

* Quick Look Consumer (client): wants to display a thumbnail or preview.
* Quick Look Producer (daemon): satisfies requests for thumbnails and
  previews using Quick Look Generators.

Producers and consumers communicate over one or more Mach ports. Allows 
crash-recovery of daemons and their termination when idle.

The producer consists of one or more Quick Look daemons (`quicklookd`) and 
multiple Quick Look generators.
The Generator interface is based on `CFPlugIn` and is specified in ANSI C.
Clients have access to the public function `QLThumbnailImageCreate` and to 
the Quick Look preview panel (`QLPreviewPanel`).
The Generator must convert document data into one of the QuickLook native 
types (plain text, rtf, html, pdf, jpg, png, tiff, etc.).

The `QLGenerator.h` file specifies the programmatic interface for 
generators. The API is broken into three categories:

1. `GenerateThumbnailForURL` and `GeneratePreviewForURL` callbacks (and 
   callbacks for cancelling generation).
2. Functions for creating graphics contexts to generate thumbnails and 
   previews.
3. Functions for returning more information about a given request.

Requests specify distinct _options_ (dictionary of hints for generation) and 
_properties_ (supplemental data).

Use `QLPreviewRequestSetDataRepresentation` with a _contentTypeUTI_ of 
`kUTTypeHTML` to render the text-based preview with the Web Kit. Use the 
`properties` dictionary to specify attachements in the HTML (images, sounds, 
etc.).

If the generator and frameworks that it uses are thread-safe, then set the 
`QLSupportsConcurrentRequests` and `QLNeedsToBeRunInMainThread` properties 
in the generator's `Info.plist` file. To handle cancellations, a generator 
should either implement the two callback functions (difficult and not 
recommended), or poll `QLThumbnailRequestIsCancelled` or 
`QLPreviewRequestIsCancelled`.

A Generator bundle must have the `.qlgenerator` extension and be in the 
filesystem at one of the following locations (in order).

* `MyApp.app/Contents/Library/Quick Look/`
* `~/Library/QuickLook`
* `/Library/QuickLook`
* `/System/Library/QuickLook`

Debugging a generator, use either

```
/usr/bin/qlmanage -t /path/to/document   # generate thumbnail
/usr/bin/qlmanage -p /path/to/document   # generate preview
/usr/bin/qlmanage -m                     # print report from daemon
defaults write -g QLEnableLogging YES    # turn on logging
```

Run with particular generator:

```
qlmanage -c org.tbrk.muttlight.email -g ~/Library/Developer/Xcode/DerivedData/muttlight-quicklook-ezvizvcnspmnffbobvbwamlodciz/Build/Products/Debug/muttlight-quicklook.qlgenerator -d4 -p test5.mbox\:2\,S

qlmanage -d4 -p test5.mbox\:2\,S  | egrep --color '.*tbrk.*|$'
```

If `qlmanage` is not displaying previews, it may be necessary to kill the 
`pboard` process.

Parsing mail headers (DONE)
---------------------------

[Bernstein notes](https://cr.yp.to/immhf.html)

Better (easier and less potential for bugs) to just integrate the Mutt code 
(GPL2)? See `parse.c` and `mutt_read_rfc822_header`.

Yes. This works well.

Interpreting and Formatting Mail Messages
-----------------------------------------

[Good summary of MIME](http://mailformat.dan.info/headers/mime.html).

To call `mutt_parse_mime_message`, we need a `HEADER` and a `CONTEXT`.

Rework `mx.c:mx_open_mailbox()` to create a `CONTEXT`.

For the `HEADER`, see `mh.c:maildir_parse_dir()` (it calls 
`mutt_new_header()`), which afterwards calls `mh.c:maildir_parse_message()`, 
which returns a `HEADER` with `ENVELOPE` (using `mutt_read_rfc822_header`).

Or just use `copy.c:mutt_copy_message()` to render the message as plain text 
in a file? It calls `handler.c:mutt_body_handler` which has a switch over 
body types.

[Viewing HTML with Mutt](https://fiasko-nw.net/~thomas/projects/htmail-view.html.en)

Opening a given message in Mutt
-------------------------------
```
mutt -f ~/tmp/testeml -e 'push <limit>~i<186B6575EF414D42849814076AE9B22B0100401380@EU-DCC-MBX01.dsone.3ds.com>\n<limit>all\n<display-message>'
```

Uniform Type Identifiers
------------------------

UTI: a string that identifies a document type.

E.g., "public.jpeg" supersedes "JPEG" OSType, ".jpg" and ".jpeg" extensions, 
and the mime type "image/jpeg".

Use reverse-DNS format, e.g., "com.apple.quicktime-movie".

They are defined in an inheritance hierarchy.

_Identifier tags_ indicate alternate methods of type identification, such as 
filename extensions, MIME types, or NSPasteboard types. You use these tags to 
assign specific extensions, MIME types, and so on, as being equivalent types in 
a UTI declaration.

Mac apps can declare new UTIs for their own proprietary formats. You declare 
new UTIs inside a bundle’s information property list.
(`UTTypeCopyDeclaringBundleURL`).
Declare in `info.plist` of an application or spotlight bundle.
Ours would be declared as an *imported* UTI.

The [.eml format](https://www.loc.gov/preservation/digital/formats/fdd/fdd000388.shtml) is for messages stored in the
[Internet Mail Format](https://www.loc.gov/preservation/digital/formats/fdd/fdd000393.shtml)
(RFC 5322).

Do we need to declare our own UTI? Or can we simply *import* the Apple UTI 
`com.apple.mail.email` and simply declare new filename-extensions?

No. This will not work, because we want our Spotlight Importer to be called and 
not Apple's. We should thus declare our own UTI (`org.tbrk.mail`? 
`org.mutt.mail`? `to.yp.cr.maildir.mail`?) and declare that it derives from 
the standard Apple one.

From `/Applications/Mail.app/Contents/Info.plist`:
```
<key>UTExportedTypeDeclarations</key>
<array>
  <dict>
    <key>UTTypeConformsTo</key>
    <array>
      <string>public.data</string>
      <string>public.email-message</string>
    </array>
    <key>UTTypeDescription</key>
    <string>Email Message</string>
    <key>UTTypeIconFile</key>
    <string>document.icns</string>
    <key>UTTypeIdentifier</key>
    <string>com.apple.mail.email</string>
    <key>UTTypeTagSpecification</key>
    <dict>
      <key>public.filename-extension</key>
      <string>eml</string>
      <key>public.mime-type</key>
      <string>message/rfc822</string>
    </dict>
  </dict>
...
</array>
```

See `UTCoreTypes.h`.

kUTTypeMessage (`public.message`, base type for messages (email, IM, etc.))
KUTTypeEmailMessage (`public.email-message`, e-mail message, conforms to `public.message`)

There are API functions to convert other type identifiers (OSType, MIME, etc) to and from UTIs.
(`UTTypeCreatePreferredIdentifierForTag`)

Dump the launch services database:
```
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -dump
```

Register the UTIs and file extensions declared in an app's `Info.plist`:
```
lsregister ~/Desktop/MyDummyApp.app
```

Preferences panel
-----------------

Build a preferences pane modelled after the `Spotlight` one.

Write it in 
[Swift](https://www.safaribooksonline.com/library/view/cocoa-programming-for/9780134077130/)?
No: stick with a mix of C and Objective C.

Show the icon (mutts sniffing around in folders), and a brief description.

Have two tabs:

* "File extensions": table view of checkboxes populated from the plist file 
  and an mdfind search.

* "About": show author names and the license details.

Need to learn basic Mac GUI concepts:

* [http://www.apress.com/us/book/9781430245421](Learn Cocoa on the Mac)
* [Preferences pane](https://developer.apple.com/library/content/documentation/UserExperience/Conceptual/PreferencePanes/Tasks/Creation.html)
* [Rows with checkboxes](https://stackoverflow.com/a/10817372)
* [Building from the commandline](https://developer.apple.com/library/content/technotes/tn2339/_index.html)

