The "Bare" directory.

This began as the NoSDK directory full of projects, but I decided in
May of 2021 to revisit all of that and make it even more self contained.
In particular, I make no reference to files that are part of the SDK.

I debated calling this "Tiny" instead of "Bare", since often the binary
images are only a few hundred bytes in size.  Note that none of this
does any kind of Wifi.  That (as far as I know) requires binary blobs
that are only available as part of the SDK.  In fact it is all that
Wifi code that produces so much bloat for simple projects that don't
need Wifi.

1. bare-1-hello - print a one line greeting.
