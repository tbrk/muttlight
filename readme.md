Muttlight
=========

Muttlight is a MacOS application that improves search and preview for email 
files stored in [MailDir](http://cr.yp.to/proto/maildir.html) format.
Specifically, it allows the associated file extensions to be specified and 
registered via a graphical user interface, provides a Spotlight importer to 
extract meta data, integrates with Quick Look to provide previews and 
thumbnails, and allows files to be opened directly in Mutt.

**This application is not yet production ready.**

**Pull requests are welcome.**

## TODO

* Improve and document the build system (including Mutt source).
* Fix the handling of environments and configuration files to avoid, for 
  example, problems finding `w3m` when previewing or opening mail files.
  Maybe include a special `muttrc` file inside the package?
* More rigorous testing and error handling.
* Reconfigure the GUI with a “Preferences” menu item that shows the window 
  with the list of extensions. Do not display this window if Muttlight is 
  launched to open a mail.
* Setup the Preferences window so that its last location is remembered 
  properly.
* Ensure that adding and removing extensions works correctly (may need to 
  launch `lsregister`? or `touch` the executable?).
  Must we retrigger an `mdimport`?

Background
----------

### Problem

[Mutt](http://www.mutt.org) is a text-based email client that works as well 
in a terminal on MacOS as on any other system. It provides an efficient, 
configurable, programmable, and keyboard-only interface, seamless 
integration of powerful texts editors like [Vim](http://www.vim.org), and a 
non-vendor-specific and easy-to-synchronize storage format, namely 
individual MIME files in [MailDirs](https://en.wikipedia.org/wiki/Maildir).

Unfortunately, mail messages stored in MailDir format are not well 
integrated into the MacOS search (Spotlight) and preview (Quick Look) 
features. Mails appear in Spotlight results, since they are indexed, but 
their encoded filenames are all but unreadable and their contents are not 
displayed. Renaming these files to have the `.eml` extension causes them to 
be properly indexed, displayed, and previewed by exploiting plugins in the 
native Mail application. But, this violates the MailDir format and renders 
the files unreadable by Mutt and similar applications.

Two possible solutions suggest themselves.

1. Introduce a version of the MailDir specification that requires `.eml` 
   extensions and modify Mutt and similar applications accordingly.

2. Somehow get the Mail application to also index these MailDir files 
   without renaming them.

The first solution is possible since Mutt is open-source software, but it 
either complicates system configuration, since Mutt must be installed with 
patches, or requires upstream acceptance of the patches. It would, 
furthermore, be necessary to patch mail delivery agents like 
[fetchmail](http://www.fetchmail.info) and 
[postfix](http://www.postfix.org). But, is it reasonably for a relatively 
minor issue on a particular operating system to spread to such long-standing 
and widely-used pieces of software?

The second solution would be ideal, but unfortunately I was not able to make 
it work. I tried registering file extensions in the finder, with the 
[duti](http://duti.org) application, and using the `UTTypeConformsTo` 
feature of MacOS applications. The Mail application is closed source and so 
it is not possible to investigate it directly or to patch it. If you know 
how to do this or you work for Apple and can influence the application, 
please [contact me](mailto://tim@tbrk.org)! I would be very pleased to make 
Muttlight redundant by finding a better solution to the problem described 
above.

### Solution

Muttlight represents a third solution: an application that registers itself 
as the owner of MailDir files and leverages Mutt's source code to improve 
their integration into the MacOS search and preview features.

A filename in the [MailDir format](https://cr.yp.to/proto/maildir.html) ends
with a suffix of the form `:2,DFPRST`, where the six final letters are flags 
that are either present or absent (giving 64 different combinations). Since 
file extensions on MacOS begin with a `.`, it is also necessary to consider 
the characters to the left of the suffix. They are sometimes `mbox`, but 
more often they are (part of) the host name on which the file was originally 
created (to ensure distinct names across multiple systems). Obviously, these 
extensions will vary across installations. Muttlight provides a GUI that 
searches the system for MailDir files, summarizes the extensions that are 
being used, and allows users to select those which should be treated. Note 
that the [DoveCot](https://wiki2.dovecot.org/MailboxFormat/Maildir) version 
of the MailDir scheme cannot be handled since it embeds file size 
information in the extension.

Once the MailDir filename ‘extensions’ have been registered to the Muttlight 
file type (`org.tbrk.muttlight.email`), the system will rely on Muttlight to 
extract metadata and provide preview images. Both features are provided by 
plugins that exploit the existing Mutt source code to parse and display mail 
messages.

Metadata is extracted from the mail header and any plain text, html, or 
enriched parts. Attachment names are indexed, but not their contents.
The various parts are decoded before being indexed. This should improve the 
quality of search results, particularly for emails in character sets other 
than ASCII. I do not know whether the Mail application indexes mail 
attachments but this seems difficult to achieve using the public Spotlight 
interfaces (since it would be necessary to recursively call other importer 
plugins).

The Quick Look plugin exploits (...hacks around...) the Mutt pager to give 
previews that, while not as elegant as the native Mail ones, closely 
resemble the text-based display in the Mutt client. This style may even be 
preferable to some users. Rudimentary parsing is applied to colorize the 
header fields, attachment status lines, and quoted replies.

Finally, the Muttlight application fields ‘open’ requests from the Finder by 
invoking a script to launch an [Iterm2](https://www.iterm2.com) session, or 
failing that, a native Terminal session, with Mutt open on the selected 
message.

Altogether, these features allow a rapid configuration and natural 
integration of MailDir contents into the MacOS user interface.

### Other Solutions

There are, of course, other ways to index and search mail messages in 
MailDir format. For instance, I already use 
[mairix](http://www.rpcurnow.force9.co.uk/mairix/) on both MacOS and Linux, 
and it works well once you get the hang of it. It has the advantage of 
working specifically with mail files and of showing the results directly 
within Mutt.

The advantage of Spotlight is that the indexes are kept up-to-date more 
dynamically and they span all files and file types on a system. Muttlight 
allows more convenient browsing of mail messages when they come up amongst 
other results (pdfs, text files, source code, etcetera).

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
It may be necessary to reset the plugin manager by running `qlmanage -r` 
(and `qlmanage -r cache`).

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

