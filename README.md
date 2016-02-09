# inputstream.mpd

This is a test addon for the new InputStream Interface.

- get the latest kodi development sources
- build the addon regarding the guidelines building kodi binary addons
- extract video.zip in data/ folder and copy/move the two files to a place kodi is able to find them
- create a .strm file with an .mpd extension (e.g. http://dummy.org/dummy.mpd)
- open the strm file in kodi

The addon does only minimal actions:
- open both .mov file previous extracted from video.zip
- process Packets in order that always the smalles DTS is passed to kodi.

methods implemented:
- OpenStream() -> Does not interpret the URL or any ListItem key/value pairs)
- GetStreamIds (returns alway 2 (1,2)
- GetStreamInfo([1,2]) returns major stream information for both streams (a/v)
- DemuxRead() Currently returnung a static DEMUXPacket Member with a static data field (TBD)

If everything works fine, there should be 20 secs of video playing and a bit longer audio.
