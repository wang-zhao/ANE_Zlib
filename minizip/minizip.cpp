/*
   minizip.c
   Version 1.1, February 14h, 2010
   sample part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications of Unzip for Zip64
         Copyright (C) 2007-2008 Even Rouault

         Modifications for Zip64 support on both zip and unzip
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )
*/
#include <atlstr.h>
#include <string>
#include <vector>

using namespace std;

typedef basic_string<TCHAR, char_traits<TCHAR>, allocator<TCHAR>> tstring;


#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64
#endif
#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BIT
#define _FILE_OFFSET_BIT 64
#endif
#endif

#ifdef __APPLE__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) _tfopen(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <utime.h>
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "zip.h"

#ifdef _WIN32
#define USEWIN32IOAPI
#include "iowin32.h"
#endif

extern CString g_err;


#define WRITEBUFFERSIZE (16384)
#define MAXFILENAME (256)

#ifdef __cplusplus
extern "C" {
#endif


int FindFiles(CString& dir, vector<tstring>& files) {

    dir.TrimRight(_T("/\\"));
    DWORD attr =  GetFileAttributes(dir);

    if (INVALID_FILE_ATTRIBUTES == attr) {
        return 0;
    }

    if ((FILE_ATTRIBUTE_DIRECTORY & attr) == 0) {
        tstring one((LPCTSTR)dir);
        files.push_back(one);
        return 1;
    } else {
        CString root(dir);

        root.AppendFormat(_T("\\*"));

        vector<tstring> subdirs;

        WIN32_FIND_DATA find_data = {0};
        HANDLE hFind = FindFirstFile(root, &find_data);

        if (hFind) {
            do {
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (!(find_data.cFileName[0] == '.' && (find_data.cFileName[1] == 0 || (
                            find_data.cFileName[1] == '.' && find_data.cFileName[2] == 0)))) {

                        CString fullsub;
                        fullsub.Format(_T("%s\\%s"), (LPCTSTR)dir, find_data.cFileName);

                        subdirs.push_back((LPCTSTR)fullsub);
                    }
                } else {
                    CString filepath;
                    filepath.Format(_T("%s\\%s"), (LPCTSTR)dir, find_data.cFileName);
                    files.push_back((LPCTSTR)filepath);
                }
            } while (FindNextFile(hFind, &find_data));

            FindClose(hFind);

            if (subdirs.size() > 0) {
                for (vector<tstring>::const_iterator it = subdirs.begin(); it != subdirs.end(); it ++) {
                    CString subdir(it->c_str());
                    FindFiles(subdir, files);
                }
            }
        }

        return files.size();
    }
}


#ifdef _WIN32
uLong filetime(
    const TCHAR* f,                /* name of file to get info on */
    tm_zip* tmzip,             /* return value: access, modific. and creation times */
    uLong* dt) {           /* dostime */
    int ret = 0;
    {
        FILETIME ftLocal;
        HANDLE hFind;
        WIN32_FIND_DATA ff32;

        hFind = FindFirstFile(f, &ff32);

        if (hFind != INVALID_HANDLE_VALUE) {
            FileTimeToLocalFileTime(&(ff32.ftLastWriteTime), &ftLocal);
            FileTimeToDosDateTime(&ftLocal, ((LPWORD)dt) + 1, ((LPWORD)dt) + 0);
            FindClose(hFind);
            ret = 1;
        }
    }
    return ret;
}
#else
#ifdef unix || __APPLE__
uLong filetime(
    char* f,               /* name of file to get info on */
    tm_zip* tmzip,         /* return value: access, modific. and creation times */
    uLong* dt) {           /* dostime */
    int ret = 0;
    struct stat s;        /* results of stat() */
    struct tm* filedate;
    time_t tm_t = 0;

    if (strcmp(f, "-") != 0) {
        char name[MAXFILENAME + 1];
        int len = strlen(f);

        if (len > MAXFILENAME) {
            len = MAXFILENAME;
        }

        strncpy(name, f, MAXFILENAME - 1);
        /* strncpy doesnt append the trailing NULL, of the string is too long. */
        name[ MAXFILENAME ] = '\0';

        if (name[len - 1] == '/') {
            name[len - 1] = '\0';
        }

        /* not all systems allow stat'ing a file with / appended */
        if (stat(name, &s) == 0) {
            tm_t = s.st_mtime;
            ret = 1;
        }
    }

    filedate = localtime(&tm_t);

    tmzip->tm_sec  = filedate->tm_sec;
    tmzip->tm_min  = filedate->tm_min;
    tmzip->tm_hour = filedate->tm_hour;
    tmzip->tm_mday = filedate->tm_mday;
    tmzip->tm_mon  = filedate->tm_mon ;
    tmzip->tm_year = filedate->tm_year;

    return ret;
}
#else
uLong filetime(
    char* f,                /* name of file to get info on */
    tm_zip* tmzip,             /* return value: access, modific. and creation times */
    uLong* dt) {           /* dostime */
    return 0;
}
#endif
#endif




int check_exist_file(
    const TCHAR* filename) {
    FILE* ftestexist;
    int ret = 1;
    ftestexist = FOPEN_FUNC(filename, _T("rb"));

    if (ftestexist == NULL) {
        ret = 0;
    } else {
        fclose(ftestexist);
    }

    return ret;
}

void do_banner_zip() {
    printf("MiniZip 1.1, demo of zLib + MiniZip64 package, written by Gilles Vollant\n");
    printf("more info on MiniZip at http://www.winimage.com/zLibDll/minizip.html\n\n");
}

void do_help_zip() {
    printf("Usage : minizip [-o] [-a] [-0 to -9] [-p password] [-j] file.zip [files_to_add]\n\n" \
           "  -o  Overwrite existing file.zip\n" \
           "  -a  Append to existing file.zip\n" \
           "  -0  Store only\n" \
           "  -1  Compress faster\n" \
           "  -9  Compress better\n\n" \
           "  -j  exclude path. store only the file name.\n\n");
}

/* calculate the CRC32 of a file,
   because to encrypt a file, we need known the CRC32 of the file before */
int getFileCrc(const TCHAR* filenameinzip, void* buf, unsigned long size_buf,
               unsigned long* result_crc) {
    unsigned long calculate_crc = 0;
    int err = ZIP_OK;
    FILE* fin = FOPEN_FUNC(filenameinzip, _T("rb"));

    unsigned long size_read = 0;
    unsigned long total_read = 0;

    if (fin == NULL) {
        err = ZIP_ERRNO;
    }

    if (err == ZIP_OK)
        do {
            err = ZIP_OK;
            size_read = (int)fread(buf, 1, size_buf, fin);

            if (size_read < size_buf)
                if (feof(fin) == 0) {
                    g_err.Format(_T("error in reading %s"), filenameinzip);
                    err = ZIP_ERRNO;
                }

            if (size_read > 0) {
                calculate_crc = crc32(calculate_crc, (const Bytef*)buf, size_read);
            }

            total_read += size_read;

        } while ((err == ZIP_OK) && (size_read > 0));

    if (fin) {
        fclose(fin);
    }

    *result_crc = calculate_crc;
    _tprintf(_T("file %s crc %lx\n"), filenameinzip, calculate_crc);
    return err;
}

int isLargeFile(const TCHAR* filename) {
    int largeFile = 0;
    ZPOS64_T pos = 0;
    FILE* pFile = FOPEN_FUNC(filename, _T("rb"));

    if (pFile != NULL) {
        int n = FSEEKO_FUNC(pFile, 0, SEEK_END);
        pos = FTELLO_FUNC(pFile);

       _tprintf(_T("File : %s is %I64d bytes\n"), filename, pos);

        if (pos >= 0xffffffff) {
            largeFile = 1;
        }

        fclose(pFile);
    }

    return largeFile;
}

int main_zip(
    int argc,
    TCHAR* argv[]) {
    USES_CONVERSION;

    int i;
    int opt_overwrite = 0;
    int opt_compress_level = Z_DEFAULT_COMPRESSION;
    int opt_exclude_path = 0;
    int zipfilenamearg = -1;
    TCHAR filename_try[MAXFILENAME + 16];
    int zipok;
    int err = ZIP_OK;
    int size_buf = 0;
    void* buf = NULL;
    const TCHAR* password = NULL;


    if (argc == 0) {
        g_err.Format(_T("invalid parameters"));
        return ZIP_PARAMERROR;
    } else {
        for (i = 0; i < argc; i++) {
            if ((*argv[i]) == '-') {
                const TCHAR* p = argv[i] + 1;

                while ((*p) != '\0') {
                    TCHAR c = *(p++);;

                    if ((c == 'o') || (c == 'O')) {
                        opt_overwrite = 1;
                    }

                    if ((c == 'a') || (c == 'A')) {
                        opt_overwrite = 2;
                    }

                    if ((c >= '0') && (c <= '9')) {
                        opt_compress_level = c - '0';
                    }

                    if ((c == 'j') || (c == 'J')) {
                        opt_exclude_path = 1;
                    }

                    if (((c == 'p') || (c == 'P')) && (i + 1 < argc)) {
                        password = argv[i + 1];
                        i++;
                    }
                }
            } else {
                if (zipfilenamearg == -1) {
                    zipfilenamearg = i ;
                }
            }
        }
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void*)malloc(size_buf);

    if (buf == NULL) {
        g_err = _T("Error allocating memory");
        return ZIP_INTERNALERROR;
    }

    if (zipfilenamearg == -1) {
        zipok = 0;
    } else {
        int i, len;
        int dot_found = 0;

        zipok = 1 ;
        _tcsncpy(filename_try, argv[zipfilenamearg], MAXFILENAME - 1);
        /* strncpy doesnt append the trailing NULL, of the string is too long. */
        filename_try[ MAXFILENAME ] = '\0';

        len = (int)_tcslen(filename_try);

        for (i = 0; i < len; i++) {
            if (filename_try[i] == '.') {
                dot_found = 1;
                break;
            }
        }

        if (dot_found == 0) {
            _tcscat(filename_try, _T(".zip"));
        }

        if (opt_overwrite == 2) {
            /* if the file don't exist, we not append file */
            if (check_exist_file(filename_try) == 0) {
                opt_overwrite = 1;
            }
        } else if (opt_overwrite == 0) {
            if (check_exist_file(filename_try) != 0) {
                g_err.Format(_T("target zip \"%s\" is existing."), filename_try);
                free(buf);
                return ZIP_INTERNALERROR;
            }
        }
    }

    if (zipok == 1) {
        zipFile zf;
        int errclose;
#        ifdef USEWIN32IOAPI
        zlib_filefunc64_def ffunc;
        fill_win32_filefunc64(&ffunc);
        zf = zipOpen2_64(filename_try, (opt_overwrite == 2) ? 2 : 0, NULL, &ffunc);
#        else
        zf = zipOpen64(filename_try, (opt_overwrite == 2) ? 2 : 0);
#        endif

        if (zf == NULL) {
            g_err.Format(_T("error opening %s"), filename_try);
            free(buf);
            return Z_STREAM_ERROR;
        }

        CString dir = argv[zipfilenamearg + 1];

        vector<tstring> files;
        FindFiles(dir, files);

        if (files.size() <= 0) {
            g_err.Format(_T("empty directory specified"));
            free(buf);
            zipClose(zf, NULL);
            return Z_DATA_ERROR;
        }

        for (vector<tstring>::const_iterator it = files.begin();
                it != files.end();
                it ++) {

            FILE* fin;
            int size_read;
            const TCHAR* filenameinzip = it->c_str();
            const TCHAR* savefilenameinzip;
            zip_fileinfo zi;
            unsigned long crcFile = 0;
            int zip64 = 0;

            zi.tmz_date.tm_sec = 0;
            zi.tmz_date.tm_min = 0;
            zi.tmz_date.tm_hour = 0;
            zi.tmz_date.tm_mday = 0;
            zi.tmz_date.tm_mon = 0;
            zi.tmz_date.tm_year = 0;
            zi.dosDate = 0;
            zi.internal_fa = 0;
            zi.external_fa = 0;
            filetime(filenameinzip, &zi.tmz_date, &zi.dosDate);

            /*
                            err = zipOpenNewFileInZip(zf,filenameinzip,&zi,
                                             NULL,0,NULL,0,NULL / * comment * /,
                                             (opt_compress_level != 0) ? Z_DEFLATED : 0,
                                             opt_compress_level);
            */
            if ((password != NULL) && (err == ZIP_OK)) {
                err = getFileCrc(filenameinzip, buf, size_buf, &crcFile);
            }

            zip64 = isLargeFile(filenameinzip);

            /* The path name saved, should not include a leading slash. */
            /*if it did, windows/xp and dynazip couldn't read the zip file. */
            savefilenameinzip = filenameinzip;

            /*should the zip file contain any path at all?*/
            if (_tcscmp((LPCTSTR)dir, filenameinzip) == 0) {
                const TCHAR* tmpptr;
                const TCHAR* lastslash = 0;

                for (tmpptr = savefilenameinzip; *tmpptr; tmpptr++) {
                    if (*tmpptr == '\\' || *tmpptr == '/') {
                        lastslash = tmpptr;
                    }
                }

                if (lastslash != NULL) {
                    savefilenameinzip = lastslash + 1; // base filename follows last slash.
                }
            } else {
                savefilenameinzip += _tcslen(dir);
            }

            while (savefilenameinzip[0] == '\\' || savefilenameinzip[0] == '/') {
                savefilenameinzip++;
            }

            /**/
            err = zipOpenNewFileInZip3_64_W(zf, savefilenameinzip, &zi,
                                          NULL, 0, NULL, 0, NULL /* comment*/,
                                          (opt_compress_level != 0) ? Z_DEFLATED : 0,
                                          opt_compress_level, 0,
                                          /* -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, */
                                          -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                          password, crcFile, zip64);

            if (err != ZIP_OK) {
                free(buf);
                zipClose(zf, NULL);
                g_err.Format(_T("error in opening %s in zipfile"), filenameinzip);
                return ZIP_INTERNALERROR;
            } else {
                fin = FOPEN_FUNC(filenameinzip, _T("rb"));

                if (fin == NULL) {
                    free(buf);
                    zipCloseFileInZip(zf);
                    zipClose(zf, NULL);
                    g_err.Format(_T("error in opening %s for reading"), filenameinzip);
                    return ZIP_ERRNO;
                }
            }

            do {
                err = ZIP_OK;
                size_read = (int)fread(buf, 1, size_buf, fin);

                if (size_read < size_buf)
                    if (feof(fin) == 0) {
                        g_err.Format(_T("error in reading %s"), filenameinzip);
                        err = ZIP_ERRNO;
                    }

                if (size_read > 0) {
                    err = zipWriteInFileInZip(zf, buf, size_read);

                    if (err < 0) {
                        g_err.Format(_T("error in writing %s in the zipfile"), filenameinzip);
                    }
                }
            } while ((err == ZIP_OK) && (size_read > 0));

            if (fin) {
                fclose(fin);
            }

            if (err < 0) {
                free(buf);
                zipCloseFileInZip(zf);
                zipClose(zf, NULL);
                return ZIP_ERRNO;
            } else {
                err = zipCloseFileInZip(zf);

                if (err != ZIP_OK) {
                    g_err.Format(_T("error in closing %s in the zipfile"), filenameinzip);
                    zipClose(zf, NULL);
                    free(buf);
                    return ZIP_ERRNO;
                }
            }
        }

        errclose = zipClose(zf, NULL);

        if (errclose != ZIP_OK) {
            g_err.Format(_T("error in closing %s"), filename_try);
            free(buf);
            return errclose;
        }
    } else {
        g_err.Format(_T("invalid parameters"));
        free(buf);
        return ZIP_ERRNO;
    }

    free(buf);
    g_err.Format(_T("compress ok"));
    return 0;
}
#ifdef __cplusplus
};
#endif
