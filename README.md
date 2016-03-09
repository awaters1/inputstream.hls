# inputstream.mpd

This is a dash mpd file addon for kodi's new InputStream Interface.

- this addon is part of the official kodi repository and part of each kodi installation
- configure the addon by adding URL prefixes wich are allowed to be played by this addon
- Open a .mpd file on your local filesystem
- or create a .strm file / or addon with passes an url with .mpd extension and open the strm file in kodi
- or write an addon wich passes .mpd files to kodi

##### Examples:
1.) mpd dash example with one video and one audio stream
- configuration URL (addon settings): http://download.tsi.telecom-paristech.fr
- URL to paste into strm file: http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live/mp4-live-mpd-AV-BS.mpd

2.) mpd dash example with one video and multiple audio streams
- configuration URL (addon settings): http://rdmedia.bbc.co.uk
- URL to paste into strm file: http://rdmedia.bbc.co.uk/dash/ondemand/testcard/1/client_manifest-events-multilang.mpd

##### Decrypting:
Decrypting is not implemented. But it is prepared!  
Decrypting takes place in separate decrypter shared libraries, wich are identified by the inputstream.mpd.licensetype listitem property.  
Only one shared decrypter library can be active during playing decrypted media. Building decrypter libraries do not require kodi sources.  
Simply check out the sources of this addon and you are able to build decrypters including full access to existing decrypters implemented in bento4.

##### TODO's:
- Adaptive bitrate switching is prepared but currently not yet activated  
Measuring the bandwidth needs some intelligence to switch not for any small network variation
- Automatic / fixed video stream selection depending on max. visible display rect (some work has to be done at the inputstream interface). Currently videos > 720p will not be selected if videos <= 720p exist.
- Currently always a full segment is read from source into memory before it is processed. This is not optimal for the cache state - should read in chunks.
- DASH implementation of periods (currently only the first period is considered)
- There will be a lot of dash mpd implementations with unsupported xml syntax - must be extended. 

##### Notes:
- On startup of a new video the average bandwidth of the previous played stream is used to choose the representation to start with. As long bandwith measurement is not fully implemented this value is fixed 4MBit/s and will not be changed.  This value can be found and modified in settings.xml, but you can also override this value using Min/Max bandwidth in the settings dialog for this addon.
- The URL entries in the settings dialog support regular expressions, please note that at least 7 chars must match the regexp to be valid. Example: http://.*.videodownload.xy supports all subdomains of videodownload.xy
- This addon is single threaded. The memory consumption is the sum of a single segment from each stream currently playing (will be reduced, see TODO's) Refering to known streams it is < 10MB for 720p videos.

##### Credits:
[@fernetmenta](github.com/fernetmenta) Best support I ever got regarding streams / codecs and kodi internals.  
[@notspiff](https://github.com/notspiff) Thanks for your ideas / tipps regarding kodi file system  
[bento4 library](https://www.bento4.com/) For me the best library choice for mp4 streams. Well written and extensible!

#####Travis CI build state: ![alt tag](https://travis-ci.org/mapfau/inputstream.mpd.svg?branch=master)  
