# inputstream.hls (1.0.0)

This is a HLS m3u8 file addon for kodi's new InputStream Interface.

To use
- create a .strm file / or addon with passes an url with .m3u8 extension and open the strm file in kodi
- or write an addon which passes .m3u8 files to kodi

##### Examples:
1.) HLS example with no encryption
- Force inputstream.hls using a Property in strm file: #KODIPROP:inputstreamaddon=inputstream.hls
- URL to paste into strm file: https://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/bipbop_4x3_variant.m3u8

2.) HLS example with AES encryption
- Force inputstream.hls using a Property in strm file: #KODIPROP:inputstreamaddon=inputstream.hls
- URL to paste into strm file: https://cdn.theoplayer.com/video/big_buck_bunny_encrypted/stream-800/index.m3u8


##### Limitations:
1. Only supports MPEG2-TS, no fragmented MP4 or packetized audio streams

##### Decrypting:
AES block level decrypting is implemented through Bento

##### Bandwidth and resolution:
When using inputstream.hls the first time, the selection of stream quality / stream resolution is done with a guess of 4MBit/s. This default value will be updated at the time you watch your first movie by measuring the download speed of the media streams.  
Always you start a new video, the average bandwidth of the previous media watched will be taken to calculate the initial stream representation from the set of existing qualities.  
If this leads to problems in your environment, you can override / adjust this value using Min. bandwidth in the inputstream.mpd settings dialog. Setting Min. bandwidth e.g. to 10.000.000, the media selection will never be done with a bandwidth value below this value.  

##### TODO's:
 

##### Notes:
The addon uses a data cache for decrypted segments and makes use of multiple threads.  Hasn't been tested on low end machines.

##### Credits:
inputstream.mpd as a base for the project
[@fernetmenta](github.com/fernetmenta) Best support I ever got regarding streams / codecs and kodi internals.  
[@notspiff](https://github.com/notspiff) Thanks for your ideas / tipps regarding kodi file system  
[bento4 library](https://www.bento4.com/) For me the best library choice for mp4 streams. Well written and extensible!

##### Continuous integration:
[![Build Status](https://travis-ci.org/awaters1/inputstream.hls.svg?branch=master)](https://travis-ci.org/awaters1/inputstream.hls)
