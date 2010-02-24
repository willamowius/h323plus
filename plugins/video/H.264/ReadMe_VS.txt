

================================================

H.264 Visual Studio

Simon Horne - Feb 2010

================================================

How to compile H.264 for Microsoft Visual Studio

To compile FFMPEG dlls you must use MinGW...
Follow the instructions found here
http://ffmpeg.arrozcru.org/wiki


Special Notes:
You must build the dlls you cannot use FFMPEG statically.
Make sure you read this
http://ffmpeg.arrozcru.org/wiki/index.php?title=MSVC

Once you have successfully compiled FFMPEG the following files will be in the 
..msys\local\bin directory.

avcodec-xx.dll
avutil-xx.dll

avcodec-xx.lib
(the numbers may vary depending on the version compiled)

In VS2008 add the following path to Visual Studio include directory path
..msys\local\include
and the following to the Visual Studio lib directory path
..msys\local\bin


To compile X264 use MinGW and follow the instructions here.
http://ffmpeg.arrozcru.org/wiki/index.php?title=X264
Note: You must use x264 ver 80 or greater. 

./configure --enable-shared

This will produce a 
libx264-xx.dll
in ..msys\local\bin directory and
libx264-xx.dll.a
in ..msys\local\lib

add following to the Visual Studio lib directory path
..msys\local\lib


Now you are ready to compile the plugin under Visual Studio.
You must also compile the h264_helper project.


Once compiled you will need to place these files in the c:\ptlib_plugin directory.

h264-x264_ptplugin.dll
x264plugin_helper.exe
avcodec-xx.dll
avutil-xx.dll
libx264-xx.dll

Don't forget to set the environmental variables
PTLIBPLUGINDIR = c:\ptlib_plugin

To stream codec trace information
PTLIB_TRACE_CODECS = 6



Simon
