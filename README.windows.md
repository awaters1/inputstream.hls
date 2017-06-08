# Setup Software

1. Download visual studio c++ build tools 2015 (http://landinghub.visualstudio.com/visual-cpp-build-tools)
2. Download git for windows (https://git-scm.com/download/win)
3. Download cmake (https://cmake.org/download/)
4. Download the Kodi source as a zip from github

# Setup Addon

1. Create the inputstream.hls directory in the addons dir  xbmc-Krypton\project\cmake\addons\addons
2. Within that directory create a file inputstream.hls.txt with the contents
```inputstream.hls https://github.com/awaters1/inputstream.hls master```
3. Also create a file platforms.txt with the contents
```all```
(see https://github.com/awaters1/inputstream.hls/tree/master/kodi/inputstream.hls for samples)

# Build Addon

1. Open the Visual C++ 2015 x86 native build tools command prompt
2. Within the Kodi source directory navigate to tools/buildsteps/win32/
3. Run the batch file make-addons.bat inputstream.hls

This will clone the repository and its dependencies and continue to run the build.  It should
put the final artifacts into xbmc-Krypton/project/Win32BuildSetup/BUILD_WIN32/addons/inputstream.hls. 
Copy that directory into the Kodi installation directory and enable within Kodi itself
