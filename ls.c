#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>

#include <windows.h>

const char * AppName           = "wls";
const char * AppVersion        = "0.6";
const char * AppSerial         = "20141010";
const char * AppYears          = "2009";
const char * AppAuthor         = "baltasarq@yahoo.es";
const char * AppArgHelp        = "--help";
const char * AppArgVerbose     = "--verbose";
const char * AppArgShowVersion = "--version";
const char * AppArgNoColors    = "--no-colors";

const char * ExtFileBackups     = " .bak .old ";
const char * ExtFileExecutables = " .exe .com .jar .py ";
const char * ExtFileCompressed  =
    " .zip .tar .tgz .gz .bz .bz2 .rar .7z .arj .lzh .lha "
    " .tbz .tbz2 .z .uu .zoo "
;

const char * ExtFileMedia       =
    /* graphics */  " .jpeg .jpg .png .bmp .gif .art .tiff .tif "
    /* sound */     " .ogg .mp3 .wma .aif .aiff .wav .pcm .mpc .flac .raw .au .midi .mid "
    /* video */     " .mpg .avi .mpeg .mpe .m1s .mpa .mov .qt .asf .asx .wmv .ogm "
                    " .3gp .mkv .ra .rm .ram .mp4 .3gp "
;

const char UserDirectoryMark = '~';
const char * CurrentDirectory   = ".";
const char * ParentDirectory    = "..";
const char * NoFilterMask       = "*.*";

const char AttrDirectory        = 'D';
const char AttrFile             = 'f';
const char AttrBackupFile       = 'b';
const char AttrHiddenFile       = 'h';

#define DirectoryEntryWidth       16386
#define FileEntryWidth            MAX_PATH

const unsigned int MaxDirectoryEntry = DirectoryEntryWidth;
const unsigned int MaxFileEntry = FileEntryWidth;
const unsigned int MaxNumEntries = 32;
const unsigned int DefaultNumColumns = 80;
bool Verbose = false;
bool ShowVersion = false;
bool NoColors = false;
unsigned int NumFiles = 0;
unsigned int NumColumns = 0;
bool * params = NULL;
char * userDirectory = NULL;
HANDLE OutputHandle;
char cwd[DirectoryEntryWidth];
char buffer[DirectoryEntryWidth];

typedef enum _Params {
    LSPViewAll, LSPLongList, LSPRecurse, LSPViewAlmostAll, LSPViewDirectoriesOnly,
    LSPIgnoreBackups, LSPSimple, LSPWide
} Params;

const char * LsParams = "alRAdB1C";
const char * LsParamsExplanation[] = {
    "view also hidden files",
    "long listing format",
    "recurse subdirectories",
    "view also hidden files, but ignore . and ..",
    "only show directories",
    "ignore backups",
    "simple - one directory entry per line",
    "wide directory listing"
};

void setUserDirectory()
{
    if ( !SUCCEEDED(
            SHGetKnownFolderPath( FOLDERID_Profile, 0, NULL, &userDirectory ) )
    {
        strcpy( userDirectory, "\\" );
    }

    return;
}

inline
void * safeMalloc(size_t size)
{
    void * toret = malloc( size );

    if ( toret == NULL ) {
        fprintf( stderr, "\nwls FATAL: not enough memory.\n" );
        exit( EXIT_FAILURE );
    }

    return toret;
}

inline bool isHiddenFile(WIN32_FIND_DATA * file);
inline bool isBackupFile(WIN32_FIND_DATA * file);
inline char getFileType(WIN32_FIND_DATA * file);

typedef struct _EntryList {
    WIN32_FIND_DATA * l;
    unsigned int numEntries;
    unsigned int capacity;
} EntryList;

inline
void entryListInit(EntryList *el)
{
    el->l = (WIN32_FIND_DATA *)
                safeMalloc( sizeof( WIN32_FIND_DATA ) * MaxNumEntries )
    ;
    el->numEntries = 0;
    el->capacity = MaxNumEntries;
}

inline
void entryListFree(EntryList *el)
{
    free( el->l );
}

inline
void entryListExpand(EntryList *el)
{
    el->capacity *= 2;

    el->l = (WIN32_FIND_DATA *)
                realloc( el->l, sizeof( WIN32_FIND_DATA ) * el->capacity )
    ;
}

inline
void entryListStore(EntryList *el, WIN32_FIND_DATA * fi)
{
    ++( el->numEntries );

    if ( el->numEntries == el->capacity ) {
        entryListExpand( el );
    }

    el->l[ el->numEntries - 1 ] = *fi;
}

inline
unsigned int getNumColumns()
{
    if ( NumColumns == 0 ) {
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;

        if ( GetConsoleScreenBufferInfo( OutputHandle, &consoleInfo ) )
                NumColumns = consoleInfo.dwSize.X;
        else    NumColumns = DefaultNumColumns;
    }

    return NumColumns;
}

typedef struct _EntryInfo {
    unsigned int index;
    WIN32_FIND_DATA * fileInfo;
    char fileType;
    unsigned int numListColumns;
    unsigned int columnWide;
} EntryInfo;

inline
void entryInfoInit(EntryInfo *ei, unsigned int maxCharsInEntry)
{
    if ( maxCharsInEntry > 0
      && getNumColumns() > maxCharsInEntry )
    {
            ei->numListColumns = ( NumColumns / maxCharsInEntry );
    } else  ei->numListColumns = 1;

    ei->columnWide = getNumColumns() / ei->numListColumns;
    ei->index = 0;
}

inline
void entryInfoSetFile(EntryInfo *ei, WIN32_FIND_DATA *fi)
{
    ei->fileInfo = fi;
    ei->fileType = getFileType( fi );
}

/* Colors */
unsigned int ScrFgBlack = 0;
unsigned int ScrFgBlue = FOREGROUND_BLUE | FOREGROUND_INTENSITY ;
unsigned int ScrFgRed = FOREGROUND_RED | FOREGROUND_INTENSITY ;
unsigned int ScrFgMagenta = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY ;
unsigned int ScrFgGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY ;
unsigned int ScrFgCyan = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
unsigned int ScrFgYellow = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
unsigned int ScrFgWhite = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
unsigned int ScrFgLightWhite = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
unsigned int ScrBgBlack = 0;
unsigned int ScrBgBlue = BACKGROUND_BLUE;
unsigned int ScrBgRed = BACKGROUND_RED;
unsigned int ScrBgMagenta = BACKGROUND_BLUE | BACKGROUND_RED;
unsigned int ScrBgGreen = BACKGROUND_GREEN;
unsigned int ScrBgCyan = BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY;
unsigned int ScrBgYellow = BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY;
unsigned int ScrBgWhite = BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;

inline void printFileEntryName(EntryInfo * ei);
inline void printFileEntryInfo(EntryInfo * ei);
void lookUpFilesInDirectory(const char * directory);

// Functionality set by command line arguments
void (*setColors)(unsigned int fg, unsigned int bg);
void (*printFileEntry)(EntryInfo *);

inline
bool streq(const char * s1, const char * s2)
{
    return ( strcmp( s1, s2 ) == 0 );
}

inline
char getLastCharacter(const char *str)
{
    return ( *( str + strlen( str ) -1 ) );
}

inline
char * makePath(char * path, const char * scndPath)
{
    if ( getLastCharacter( path ) != '\\' ) {
        strcat( path, "\\" );
    }

    return strcat( path, scndPath );
}

inline
bool isDirectory(WIN32_FIND_DATA * file)
{
    return ( file->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY );
}

inline
char getFileType(WIN32_FIND_DATA * file)
{
    char toret = AttrFile;

    if ( isHiddenFile( file ) )
    {
        toret = AttrHiddenFile;
    }
    else
    if ( isDirectory( file ) ) {
        toret = AttrDirectory;
    }
    else
    if ( isBackupFile( file ) ) {
        toret = AttrBackupFile;
    }

    return toret;
}

inline
bool showAll()
{
    return ( params[ LSPViewAll ] || params[ LSPViewAlmostAll ] );
}

inline
bool isCurrentOrParentDirectory(const char * fileName)
{
    return ( streq( fileName, CurrentDirectory )
          || streq( fileName, ParentDirectory ) )
    ;
}

inline
bool isFileToInclude(WIN32_FIND_DATA *findFileData)
{
    if ( isHiddenFile( findFileData )
      && !showAll() )
    {
        return false;
    }

    if ( !isDirectory( findFileData )
      && params[ LSPViewDirectoriesOnly ] )
    {
        return false;
    }

    if ( params[ LSPIgnoreBackups ]
      && isBackupFile( findFileData ) )
    {
        return false;
    }

    if ( params[ LSPViewAlmostAll ]
      && isCurrentOrParentDirectory( findFileData->cFileName ) )
    {
        return false;
    }

    return true;
}

inline
void convtSlashes(char *path)
{
    for(; *path != '\0'; ++path) {
        if ( *path == '/' ) {
            *path = '\\';
        }
    }
}

char * getFileExt(char * buffer, const char * fileName)
{
    char * ext = strrchr( fileName, '.' );

    // Copy extension to buffer
    *buffer = 0;
    if ( ext != NULL ) {
        strcpy( buffer, ext );
    }

    // Change case to lower
    for(ext = buffer; *ext != '\0'; ++ext) {
        *ext = tolower( *ext );
    }

    return buffer;
}

inline
bool isBackupFile(WIN32_FIND_DATA * file)
{
    char buffer[MaxFileEntry];
    bool toret = false;
    const char * fileName = file->cFileName;

    if ( !( file->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ) {
        toret = ( getLastCharacter( fileName ) == '~' );

        if ( !toret ) {
            char * fileExt = getFileExt( buffer, fileName );
            if ( *fileExt != 0 ) {
                toret = ( strstr( ExtFileBackups, fileExt ) != NULL );
            }
        }
    }

    return toret;
}

inline
bool isHiddenFile(WIN32_FIND_DATA * file)
{
    bool toret = ( file->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN );

    if ( !toret ) {
        toret = ( file->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM );

        if ( !toret ) {
            toret = !isCurrentOrParentDirectory( file->cFileName )
                 && ( *( file->cFileName ) == '.' )
            ;
        }
    }

    return toret;
}

inline
SYSTEMTIME getLastAccessTime(WIN32_FIND_DATA * file)
{
    SYSTEMTIME utcTime;
    SYSTEMTIME localTime;

    FileTimeToSystemTime( &( file->ftLastWriteTime ), &utcTime );
    SystemTimeToTzSpecificLocalTime( NULL, &utcTime, &localTime );

    return localTime;
}

char * getLastAccessTimeAsString(char * buffer, WIN32_FIND_DATA * file)
{
    SYSTEMTIME date = getLastAccessTime( file );

    sprintf( buffer, "%02d/%02d/%04d %02d:%02d:%02d",
             date.wDay, date.wMonth, date.wYear,
             date.wHour, date.wMinute, date.wSecond
    );

    return buffer;
}

inline
void setConsoleColumnTo(unsigned int x)
{
    COORD coord;
    CONSOLE_SCREEN_BUFFER_INFO cursorInfo;

    // Get current line number
    if ( GetConsoleScreenBufferInfo( OutputHandle, &cursorInfo ) )
    {
        // Change cursor position
        coord.Y = cursorInfo.dwCursorPosition.Y;
        coord.X = x;
        SetConsoleCursorPosition( OutputHandle, coord );
    }
    else printFileEntry = printFileEntryName;
}

inline
void setConsoleColors_doNothing(unsigned int fg, unsigned int bg)
{
}

inline
void setConsoleColors(unsigned int fg, unsigned int bg)
{
    SetConsoleTextAttribute( OutputHandle, bg | fg );
}

inline
void setDefaultConsoleColors()
{
    setColors( ScrFgWhite, ScrBgBlack );
}

inline
void setConsoleColorsFromFileType(EntryInfo * ei)
{
    if ( ei->fileType == AttrDirectory ) {
        setColors( ScrFgBlue, ScrFgBlack );
    }
    else
    if ( ei->fileType == AttrFile ) {
        char buffer[MaxFileEntry];

        /* Prepare ext */
        getFileExt( buffer, ei->fileInfo->cFileName );

        if ( *buffer != 0 ) {
            strcat( buffer, " " );

            /* Select colors */
            if ( strstr( ExtFileExecutables, buffer ) != NULL ) {
                setColors( ScrFgYellow, ScrFgBlack );
            }
            else
            if ( strstr( ExtFileCompressed, buffer ) != NULL ) {
                setColors( ScrFgMagenta, ScrFgBlack );
            }
            else
            if ( strstr( ExtFileMedia, buffer ) != NULL ) {
                setColors( ScrFgCyan, ScrFgBlack );
            }
        }
    }
    else
    if ( ei->fileType == AttrHiddenFile ) {
        setColors( ScrFgRed, ScrFgBlack );
    }
    else
    if ( ei->fileType == AttrBackupFile ) {
        setColors( ScrFgGreen, ScrFgBlack );
    }
}

inline
void printBold(const char * msg)
{
    setColors( ScrFgLightWhite, ScrBgBlack );

    printf( "%s\n", msg );

    setDefaultConsoleColors();
}

inline
void printFileEntryName(EntryInfo * ei)
{
    printf( "%s\n", ei->fileInfo->cFileName );
}

inline
void printFileEntryInfo(EntryInfo * ei)
{
    char buffer[MaxFileEntry];
    unsigned long int fileSize =
            ( ei->fileInfo->nFileSizeHigh * ( MAXDWORD + 1 ) )
          + ei->fileInfo->nFileSizeLow
    ;

    printf( "%12lu %s %c %s\n",
            fileSize,
            getLastAccessTimeAsString( buffer, ei->fileInfo ),
            ei->fileType,
            ei->fileInfo->cFileName
    );
}

inline
void printFileEntryNameWide(EntryInfo * ei)
{
    unsigned int pos = ei->index % ei->numListColumns;

    if ( pos == 0 ) {
        printf( "\n" );
    }

    setConsoleColumnTo( pos * ei->columnWide );

    printf( "%s", ei->fileInfo->cFileName );
    ++( ei->index );
}

void showFile(EntryInfo * ei)
{
    // Show file info
    setConsoleColorsFromFileType( ei );
    printFileEntry( ei );
    setDefaultConsoleColors();

    return;
}

void readDirectory(
        const char * directoryName,
        EntryList * lFiles,
        EntryList * lDirs,
        unsigned int * maxCharsInEntry)
{
    WIN32_FIND_DATA findFileData;
    HANDLE hFind;

    entryListInit( lDirs );
    entryListInit( lFiles );
    *maxCharsInEntry = 0;

    // Locate first entry
    hFind = FindFirstFile( directoryName, &findFileData );
    if ( hFind == INVALID_HANDLE_VALUE ) {
        printf( "\tNo files match.\n\n" );
        goto end;
    }

    // Locate remaining entries
    do {
        if ( !isFileToInclude( &findFileData ) ) {
            continue;
        }

        unsigned int fileNameLength = strlen( findFileData.cFileName );

        if ( fileNameLength > *maxCharsInEntry ) {
            *maxCharsInEntry = fileNameLength;
        }

        if ( isDirectory( &findFileData ) )
                entryListStore( lDirs, &findFileData );
        else    entryListStore( lFiles, &findFileData );
    } while( FindNextFile( hFind, &findFileData ) );

    FindClose( hFind );

    end:
    ++( *maxCharsInEntry );
    return;
}

inline
bool isDrive(const char * directory)
{
    return ( strlen( directory ) == 2
          && isalpha( *directory )
          && *( directory + 1 ) == ':' )
    ;
}

void readFiles(const char ** files,
        EntryList * lFiles,
        EntryList * lDirs,
        unsigned int * maxCharsInEntry)
{
    register unsigned int i;
    const char ** file = files;
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    entryListInit( lDirs );
    entryListInit( lFiles );
    *maxCharsInEntry = 0;

    for(file = files, i = 0; i < NumFiles; ++i, ++file) {
        /* Pre-check: these are not taken by Win32 API */
        if ( isDrive( * file ) ) {
            memset( &findFileData, 0, sizeof( findFileData ) );
            strcpy( findFileData.cFileName, *file );
            strcat( findFileData.cFileName, "\\" );
            findFileData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            entryListStore( lDirs, &findFileData );
        }
        else
        if ( isCurrentOrParentDirectory( *file )
          || streq( *file, "/" )
          || streq( *file, "\\" ) )
        {
            memset( &findFileData, 0, sizeof( findFileData ) );
            strcpy( findFileData.cFileName, *file );
            findFileData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            entryListStore( lDirs, &findFileData );
        }
        else {
            strcpy( buffer, *file );
            convtSlashes( buffer );
            char ch = getLastCharacter( buffer );

            if ( ch == '\\' ) {
                *( buffer + ( strlen( buffer ) -1 ) ) = 0;
            }

            /* Check whether is a directory or a file */
            hFind = FindFirstFile( buffer, &findFileData );
            if (hFind == INVALID_HANDLE_VALUE) {
                printf( "\tNo files match.\n" );
                continue;
            }

            if ( !isFileToInclude( &findFileData ) ) {
                continue;
            }

            unsigned int fileNameLength = strlen( findFileData.cFileName );

            /* Store accordingly */
            if ( isDirectory( &findFileData ) ) {
                    fileNameLength = strlen( buffer );
                    strcpy( findFileData.cFileName, buffer );
                    entryListStore( lDirs, &findFileData );
            } else entryListStore( lFiles, &findFileData );

            /* Chk entry width */
            if ( fileNameLength > *maxCharsInEntry ) {
                *maxCharsInEntry = fileNameLength;
            }

            FindClose(hFind);
        }
    }

    ++( *maxCharsInEntry );
}

void processEntries(
    const char * directory,
    EntryList *lFiles,
    EntryList *lDirs,
    unsigned int maxNumCharsinEntry)
{
    WIN32_FIND_DATA *fi;
    register unsigned int i;
    EntryInfo entryInfo;

    /* Prepare screen info */
    entryInfoInit( &entryInfo, maxNumCharsinEntry );

    /* First list files */
    for(i = 0, fi = lFiles->l; i < lFiles->numEntries; ++i, ++fi) {
        entryInfoSetFile( &entryInfo, fi );
        showFile( &entryInfo );
    }

    /* Now list dirs (recurse if needed) */
    for(i = 0, fi = lDirs->l; i < lDirs->numEntries; ++i, ++fi) {
        if ( isHiddenFile( fi )
          && !showAll() )
        {
            continue;
        }

        if ( params[ LSPRecurse ]
         && !isCurrentOrParentDirectory( fi->cFileName ) )
        {
            strcpy( buffer, directory );
            lookUpFilesInDirectory( makePath( buffer, fi->cFileName ) );
        } else {
            entryInfoSetFile( &entryInfo, fi );
            showFile( &entryInfo );
        }
    }

    printf( "\n" );
}

void lookUpFilesInDirectory(const char * directory)
{
    EntryList lDirs;
    EntryList lFiles;
    unsigned int maxNumCharsinEntry = 0;
    char dir[MaxDirectoryEntry];

    /* Prepare file name */
    strcpy( buffer, directory );
    convtSlashes( buffer );

    /* Don't ask me why, but the bare '\' does not work */
    if ( streq( buffer, "\\" ) ) {
        strcat( buffer, "." );
    }

    strcpy( dir, buffer );
    makePath( buffer, NoFilterMask );

    /* Print directory's path */
    printf( "\n" );
    printBold( dir );
    printf( "\n" );

    /* Find files & list */
    readDirectory( buffer, &lFiles, &lDirs, &maxNumCharsinEntry );
    processEntries( dir, &lFiles, &lDirs, maxNumCharsinEntry );

    /* clean up */
    entryListFree( &lDirs );
    entryListFree( &lFiles );
    return;
}

void lookUpFiles(const char ** files)
{
    unsigned int maxNumCharsinEntry;
    register unsigned int i;
    WIN32_FIND_DATA * fi;

    /* Read Cwd */
    unsigned int numCharsCwd = GetCurrentDirectory( MaxDirectoryEntry, cwd );

    if ( numCharsCwd == 0 ) {
        fprintf( stderr, "wls ERROR: retrieving working directory\n" );
        goto end;
    }

    /* Trigger listing */
    if ( NumFiles == 0 ) {
        lookUpFilesInDirectory( cwd );
    } else {
        EntryList lDirs;
        EntryList lFiles;

        readFiles( files, &lFiles, &lDirs, &maxNumCharsinEntry );

        if ( lFiles.numEntries > 0
          || params[ LSPViewDirectoriesOnly ] )
        {
            processEntries( cwd, &lFiles, &lDirs, maxNumCharsinEntry );
        } else {
            for(i = 0, fi = lDirs.l; i < lDirs.numEntries; ++i, ++fi) {
                lookUpFilesInDirectory( fi->cFileName );
            }
        }

        entryListFree( &lDirs );
        entryListFree( &lFiles );
    }

    end:
    return;
}

inline
void printTitle()
{
    printf( "%s %s %s\n\n", AppName, AppVersion, AppSerial );
}

void printSintax()
{
    register unsigned int i;

    printf( "\t%s %s is (c) %s %s\n\n", AppName, AppVersion, AppAuthor, AppYears );
    printf( "\tCommand line arguments:\n" );

    for(i = 0; i < strlen( LsParams ); ++i) {
        printf( "\t-%c: %s\n", LsParams[ i ], LsParamsExplanation[ i ] );
    }

    printf( "%c", '\n' );

    printf( "\n\t--no-colors\tAvoid the use of colors." );
    printf( "\n\t--version\tShows version and exits." );
    printf( "\n\t--verbose\tShows program title before directory listing." );
    printf( "\n\t--help\t\tShows this help and exits." );

    printf( "%c", '\n' );

    exit( EXIT_FAILURE );
}

void processArg(const char ** files, const char * arg)
{
    register unsigned int i;

    if ( *arg == '-' ) {
        if ( streq( arg, AppArgHelp ) ) {
            printSintax();
        }

        if ( streq( arg, AppArgVerbose ) ) {
            Verbose = true;
            goto end;
        }

        if ( streq( arg, AppArgShowVersion ) ) {
            ShowVersion = true;
            goto end;
        }

        if ( streq( arg, AppArgNoColors ) ) {
            NoColors = true;
            goto end;
        }

        // Discard preceding '-'
        while( *arg == '-' ) {
            ++arg;
        }

        for(i = 0; i < strlen( arg ); ++i) {
            char * ptr = strchr( LsParams, arg[ i ] );

            if ( ptr != NULL )
                    params[ ptr - LsParams ] = true;
            else    printSintax();

        }
    } else {
        files[ NumFiles ] = arg;
        ++NumFiles;
    }

    end:
    return;
}

inline
void finalArgChecks()
{
    if ( params[ LSPLongList ] ) {
        params[ LSPSimple ] = false;
        params[ LSPWide ] = false;
    }
    else
    if ( params[ LSPSimple ] ) {
        params[ LSPWide ] = false;
        params[ LSPLongList ] = false;
    }

    // Set pointers to functionality
    if ( NoColors )
            setColors = setConsoleColors_doNothing;
    else    setColors = setConsoleColors;

    if ( params[ LSPLongList ] )
            printFileEntry = printFileEntryInfo;
    else    printFileEntry = printFileEntryName;

    if ( params[ LSPWide ] ) {
        printFileEntry = printFileEntryNameWide;
    }

    // Prepare
    OutputHandle = GetStdHandle( STD_OUTPUT_HANDLE );

    if ( OutputHandle == INVALID_HANDLE_VALUE
      || OutputHandle == NULL )
    {
        NoColors = true;
        params[ LSPLongList ] = false;
        params[ LSPWide ] = false;
        params[ LSPSimple ] = true;
    }
}

void cnvtHomeDirectory(char ** files)
{
    for(int i = 0; i < NumFiles; ++i) {
        char * tildePos = strchr( files[ i ], UserDirectory );

        if( tildePos != NULL ) {

        }
    }

    return;
}

int main(int argc, char * argv[])
{
    unsigned int i;

     // Prepare
    setUserDirectory();
    params = (bool *) safeMalloc( sizeof( bool ) * strlen( LsParams ) );
    memset( params, 0, sizeof( bool ) * strlen( LsParams ) );
    const char ** files = (const char **) safeMalloc( sizeof( char * ) * argc );

    // Process arguments
    params[ LSPWide ] = true;
    for(i = 1; i < argc; ++i) {
        processArg( files, argv[ i ] );
    }
    finalArgChecks( params );
    cnvtHomeDirectory( files );

    if ( ShowVersion ) {
        printTitle();
        goto end;
    }

    // Launch
    if ( Verbose ) {
        printTitle();
    }

    lookUpFiles( files );

    end:
    free( params );
    free( files );
    CoTaskMemFree( userDirectory );
    return EXIT_SUCCESS;
}

