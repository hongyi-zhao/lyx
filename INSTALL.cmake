=========================
Building LyX with CMake
=========================

    Install CMake from www.cmake.org or your distribution (version >= 3.1.0).



3rd party libraries
--------------------

    Install Qt 5 or Qt 6 and make sure qmake is found.
        Add the folder with qmake to the environment variable PATH.
        If you've compiled Qt by yourself or qmake is not found after
        installing Qt fix PATH,
        Linux/Unix: export PATH=<your path to qt>/bin:$PATH
        Windows   : set PATH=<your path to qt>\bin;%PATH%
        if you use -DLYX_USE_QT=AUTO, or do not use LYX_USE_QT, Qt 6 will be searched first
        and if not found Qt 5 will be used.
        If you want to use Qt 5, use -DLYX_USE_QT=QT5.

    When you want to run LyX without installing from an out-of-source
    build directory you have to set the environment variable LYX_DIR_24x
    and it must point to the lib dir in the source tree.
        Linux/Unix: export LYX_DIR_24x=<lyx-source>/lib
        Windows   : set LYX_DIR_24x=<lyx-source>\lib

    Windows specific

        On Windows install the supplementary modules:
        * Visual Studio 2015: http://ftp.lyx.org/pub/lyx/devel/win_deps/lyx-windows-deps-msvc2015.zip
                or use the option LYX_DEPENDENCIES_DOWNLOAD, then you have to add
                these paths to your PATH variable:
                  <build-dir>\lyx-windows-deps-msvc2015\bin
                  <build-dir>\lyx-windows-deps-msvc2015\Python
                  <build-dir>\lyx-windows-deps-msvc2015\imagemagick
                  <build-dir>\lyx-windows-deps-msvc2015\ghostscript
                  <build-dir>\lyx-windows-deps-msvc2015\gettext-tools

        If cmake couldn't find these modules set GNUWIN32_DIR, e.g.
        -DGNUWIN32_DIR=c:\gnuwin32. By default cmake searches in your 
        program folder. Or use the cmake GUI to set the GNUWIN32_DIR path.

        The build process tries to find aspell on Windows
        in %ProgramFiles%/GnuWin32/ and in /usr/ or in /usr/local 
        under Linux. If it could not find aspell, spell checking
        will be disabled.



Generating build system files
------------------------------

    CMake is a build system file generator. On all systems it could
    generate files for several build systems, for instance Makefiles
    for make, project files for Visual Studio, Xcode, Eclipse.

    Running cmake without any argument lists all supported build
    systems on your system. Passing one of them as -G"<build system name>" 
    argument when running cmake selects this.



Building out-of-source
-----------------------

    The standard way of using CMake is to build in a folder that doesn't reside
    in the source tree. This has the advantage that a complete fresh build could
    be done by simply deleting all files in the build folder and re-running cmake
    again.

    Another benefit of out-of-source builds is that several builds (debug, release,
    command-line builds, IDE project files) could all use the same source tree.

    Therefore when using CMake create a folder outside of the source tree and
    select this folder when using CMake's GUI, cmake-gui, or go into this folder
    when you call cmake from the shell.

    Before performing an out-of-source build, ensure that all CMake generated
    in-source build information is removed from the source directory,
    e.g., CMakeFiles directory, CMakeCache.txt.



Using cmake
------------

    When calling cmake you must pass the path to the source tree (absolute or relative)
    and optionally the generator (each system has its own default). Additional arguments
    could be passed with the -D prefix.

    Here some examples, assuming the build folder is in the same folder as the source tree:

    * Makefiles on Linux
        cmake ../trunk

    * Project files for QtCreator:
        Open the trunk/CMakeLists.txt file and select the build folder
        or create the files in the command line using the -G"CodeBlocks *" option, eg
            cmake ../trunk -G"CodeBlocks - Unix Makefiles"

    * Project files for Xcode
        cmake ../trunk -GXcode

    * Project files for Visual Studio 10
        cmake ..\trunk -G"Visual Studio 10"

    * NMake files for Visual Studio
        cmake ..\trunk -G"NMake Makefiles"

    * Makefiles for MinGW
        cmake ..\trunk -G"MinGW Makefiles"


    Daily work:

    * Re-running cmake is simple
        cmake .

    * Adding new files 
        The cmake build system scans the directories, so no need to update any file,
        just re-run cmake. Also the mocing rules are generated.

    * Unused source code file
        Because cmake scans the directories for *.cpp and *.h files it will also
        add files to the build system that are not mentioned to build. To exclude
        them change their ending and re-run cmake.




Build options
--------------

    Options could be passed by the -D prefix when running cmake.
    Available options will be listed on each cmake run.
    "cmake -Dhelp=1 -S <lyx-source-dir> -B /tmp" lists all available options:
    !!! Mark, that using this command creates CMakeCache.txt and CMakeFiles
    !!! which have to be removed afterwards. Therefore it is better to explicitly
    !!! specify a temporary build directory (/tmp in this example)
    

    # Available on all systems/compilers
    -- LYX_CPACK                = OFF    : Use the CPack management (Implies LYX_INSTALL option)
    -- LYX_LOCALVERSIONING      = ON     : Add version info to created package name (only used if LYX_CPACK option set)
    -- LYX_INSTALL              = OFF    : Build install projects/rules (implies a bunch of other options)
    -- LYX_NLS                  = ON     : Use nls
    -- LYX_REQUIRE_SPELLCHECK   = OFF    : Abort if no spellchecker available
    -- LYX_ASPELL               = OFF    : Require aspell
    -- LYX_ENCHANT              = OFF    : Require Enchant
    -- LYX_HUNSPELL             = OFF    : Require Hunspell
    -- LYX_RELEASE              = OFF    : Build release version, build debug when disabled
    -- LYX_DEBUG                = ON     : Enforce debug build
    -- LYX_NO_OPTIMIZE          = OFF    : Don't use any optimization/debug flags
    -- LYX_PACKAGE_SUFFIX       = ON     : Use version suffix for packaging
    -- LYX_PCH                  = OFF    : Use precompiled headers
    -- LYX_MERGE_FILES          = OFF    : Merge source files into one compilation unit
    -- LYX_MERGE_REBUILD        = OFF    : Rebuild generated files from merged files build
    -- LYX_QUIET                = OFF    : Don't generate verbose makefiles
    -- LYX_INSTALL_PREFIX       = OFF    : Install path for LyX
    -- LYX_BUNDLE               = OFF    : Build bundle  (experimental) 
    -- LYX_ENABLE_URLTESTS      = OFF    : Enable for URL tests
    -- LYX_ENABLE_EXPORT_TESTS  = ON     : Enable for export tests
    -- LYX_ENABLE_KEYTESTS      = ON     : Enable for keytests
    -- LYX_ASAN                 = OFF    : Use address sanitizer
    -- LYX_USE_FILEDIALOG       = NATI   : Use native or QT file dialog (QT NATIVE)
    -- LYX_USE_QT               = QT6    : Use Qt version as frontend (AUTO QT5 QT6)
    -- LYX_DISABLE_CALLSTACK_PRI= OFF    : do not print a callstack when crashing
    -- LYX_EXTERNAL_Z           = ON     : OFF := Build 3rdparty lib zlib
    -- LYX_EXTERNAL_ICONV       = ON     : OFF := Build 3rdparty lib iconvlib
    -- LYX_EXTERNAL_HUNSPELL    = OFF    : OFF := Build 3rdparty lib hunspelllib
    -- LYX_EXTERNAL_MYTHES      = OFF    : OFF := Build 3rdparty lib mytheslib (AUTO OFF ON)

    # GCC specific 
    -- LYX_PROFILE              = OFF    : Build with options for gprof
    -- LYX_EXTERNAL_BOOST       = OFF    : Use external boost
    -- LYX_PROGRAM_SUFFIX       = ON     : Append version suffix to binaries
    -- LYX_DEBUG_GLIBC          = OFF    : Enable libstdc++ debug mode
    -- LYX_DEBUG_GLIBC_PEDANTIC = OFF    : Enable libstdc++ pedantic debug mode
    -- LYX_STDLIB_DEBUG         = OFF    : Use debug stdlib

    # MSVC specific
    -- LYX_CONSOLE              = ON     : Show console on Windows
    -- LYX_VLD                  = OFF    : Use VLD with MSVC
    -- LYX_WALL                 = OFF    : Enable all warnings
    -- LYX_DEPENDENCIES_DOWNLOAD= OFF    : Download precompiled 3rd party libraries for MSVC 10

    # APPLE specific
    -- LYX_DMG                  = OFF    : Build as Mac bundle, needed for .dmg  (experimental)
    -- LYX_COCOA                = OFF    : Use Cocoa on Mac


Using the merged files build (Deprecated)
-----------------------------

    When the option 'LYX_MERGE_FILES' is used then for each library a file
    is generated that includes all source files of this library, this speeds
    up compilation by a factor of about 5.

    When you heavily work on one file you could comment out the relevant
    define in the '_allinone_const.C' file, so only the file _allinone_touched.C'
    will be re-compiled again and again.


    - Adding new files
      When you add new files the merging files have to be rebuilt:
        cmake -DLYX_MERGE_REBUILD=1 .
      Or start over by completely cleaning the build folder.

    - Starting over with the same configuration
      Delete all files except CMakeCache.txt and call
        cmake .



Visual Studio C++
------------------

    * Warnings: The default warning level of the msvc cmake builds 
      is /W3. To enable /W4 use
        '-DLYX_WALL=1 '
      and
        '-DLYX_WALL=0'
      switches back to to /W3.
      To disable a specific warning add it to MSVC_W_DISABLE in
      cmake/CMakeLists.txt. To make the warning an error add it
      to MSVC_W_ERROR.

    * Memory leak detection
      For MSVC the usage of 'Visual Leak Detection' could be enabled
      (http://dmoulding.googlepages.com/vld): -DLYX_VLD=1

      Building vld requires the 'Debugging Tools For Windows' (~16MB)
      http://www.microsoft.com/whdc/devtools/debugging/default.mspx
      Eventually you have to copy dbghelp.dll from your Visual Studio
      Common7\IDE folder into the binary directory.
      'vld.ini' is part of lyx.proj where you can configure vld.

      The documentation for vld can be found in development/Win32/vld, or
      http://www.codeproject.com/tools/visualleakdetector.asp


    Some tips:

    * the Release build links much faster
    * for the 'Debug' and 'Release' build all precompiled headers are enabled
      to compile without pch (non file merge mode) This could be used to check
      if all necessary headers are included.
        * use 'MinSizeRel' which only precompiles the STL and Boost headers
        * use 'RelWithDebInfo' which does not use any precompiled headers



GCC/Windows (Win2k only works with MSYS, XP?)
----------------------------------------------
    QMAKESPEC is needed:
      export QMAKESPEC=win32-g++ (MSYS) or set QMAKESPEC=win32-g++ (CMD)



Ubuntu/Kubuntu
---------------

    You need additionally these packages:
      * g++
      * cmake
      * qttools5-dev-tools + qt5-qmake or qt6-base-dev-tools + qt6-tools-dev



Xcode/Mac
----------

    Some tips:

    * Xcode prefers UTF8 when opening source files, though LyX usually uses
      Latin1. To fix that select all source files in Xcode and click "Get Info"
      in the context menu. Change the encoding to Latin1.

    * You can run and debug LyX from Xcode. For LyX to find its resources, there
      are two possibilities:
        a) Put a resource directory, e.g. a link to the lib directory of the
           source tree, at .../trunk/../build/bin/Resources
        b) Select the lyx-qt4 executable in Xcode, click on "Get Info" in the 
           context menu and add "-sysdir a_valid_LyX_resource_directory"
           pointing e.g. to a valid Contents/Resources of a LyX.app directory.

    * LyX on Mac doesn't look for fonts in the resource directory if the
      executable is not in an .app bundle. Instead you have to create a
      symbolic link to the fonts directory in the place where the executable
      is: ln -s .../trunk/lib/fonts .../trunk/../build/bin/Debug/
      If you don't do that math characters will not show up correctly.

    * CMake properly finds the Qt4 library bundles from Trolltech's binary
      Qt4 package for Mac. So no need to compile Qt on your own.



Updating the translations
--------------------------

    Read README.localization for information on the translation process.
    Here we list only the CMake specific part. As with the autotools build
    system the translations are only updated on demand.

    - Update .po files and create .gmo files (does not touch the source tree):
        make translations

    - Copy the updated .po files into the source tree for committing:
        make update-po

    - Update the layouttranslations file in the source tree:
        make layouttranslations1



Packaging
----------

    - Source .tar.gz, .tar.bz2, .zip:
        make package_source

    - Binary .tar.gz and install .sh:
        make package

    - Binary .deb:
	Configure with "cmake -DCPACK_BINARY_DEB:BOOL=ON"
	  make package
	-- or --
	  create : cpack -G DEB --config CPackConfig.cmake
        list   : dpkg-deb -c lyx-*.deb
        install: dpkg -i lyx-*.deb

    - Binary .rpm:
	Configure with "cmake -DCPACK_BINARY_RPM:BOOL=ON"
	  make package
	-- or --
	  create : cpack -G RPM --config CPackConfig.cmake
        list   : rpm -qlp lyx-*.rpm
        install: rpm -U lyx-*.rpm

