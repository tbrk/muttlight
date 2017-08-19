Muttlight
=========

Muttlight is a MacOS application that improves search and preview for email 
files stored in [MailDir](http://cr.yp.to/proto/maildir.html) format.
Specifically, it allows the associated file extensions to be specified and 
registered via a graphical user interface, provides a Spotlight importer to 
extract meta data, and integrates with Quick Look to provide previews and 
thumbnails.

Debugging
---------

Run `mdimport -L` to see the list of installed Spotlight plugins.
The list should include 
`.../muttlight.app/Contents/Library/Spotlight/muttlight.mdimporter`.

Run `mdls` on a MailDir file to check that the plugin is working correctly.
The `kMDItemContentType` field should be `org.tbrk.muttlight.email` and the 
other fields (`kMDItemAuthors`, `kMDItemDisplayName`, etcetera) should be 
valid. To log the importing process and to see whether the plugin is being 
called, run `mdimport -d 4 <filename>`.

Run `qlmanage -m | egrep --color '.*muttlight.*|$` to see the list of 
installed Quick Look plugins (and to highlight the muttlight entry).
The list should include
`org.tbrk.muttlight.email -> .../muttlight.app/Contents/Library/QuickLook/muttlight.qlgenerator`.
It may be necessary to reset the plugin manager by running `qlmanage -r`.

Open a MailDir directory in Finder to check that thumbnails are being 
generated correctly, press `⌘-Y` to preview a file. The thumbnails and 
previews should show the messages more or less as they appear in mutt.

The Quick Look plugin can be debugged by running
`qlmanage -d 4 -p <filename>`. This will show detailed logging. There may be 
a delay before the preview is displayed. Add `-o .` to the command line to 
dump the generated files to disk.

Run `mdfind <keywords>` to list indexed files that contain the given 
keywords. Entering the same keywords into the Spotlight search box 
(`⌘-Space`) should display the same list. When muttlight is functioning 
correctly, any mail files in the search results are named according to their 
subject (and not their MailDir filename) and their contents should be 
previewed as in the mutt pager. It will probably be necessary to 
[reindex](https://support.apple.com/en-us/HT201716) your MailDir directories 
before the search results are named correctly (the `mds` and `mds_stores` 
processes become active during reindexing).

Run the following command and search for `muttlight` to see registered file 
extensions and plugin “claims”.

```
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister -dump | less
```

