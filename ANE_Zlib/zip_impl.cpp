#include <atlstr.h>
#include "zip_impl.h"
#include <io.h>

CString g_err;

#ifdef __cplusplus
extern "C" {
#endif

extern int main_zip(
    int argc,
    TCHAR* argv[]);


extern int main_unz(
    int argc,
    char* argv[]);

BOOL Zip(CString& src, CString& dst) {

    if (_taccess((LPCTSTR)src, 0) != 0) {
        g_err.Format(_T("source directory \"%s\" is not existing."), (LPCTSTR)src);
        return FALSE;
    }

    TCHAR* argv[] = {
        _T("-9"),
        _tcsdup((LPCTSTR)dst),
        _tcsdup((LPCTSTR)src)
    };

    int nret = main_zip(3, argv);
    free(argv[1]);
    free(argv[2]);

    return nret == 0;
}
BOOL UnZip(CString& src, CString& dst) {
    if (_taccess((LPCTSTR)src, 0) != 0) {
        g_err.Format(_T("source zip file \"%s\" is not existing."), (LPCTSTR)src);
        return FALSE;
    }


    USES_CONVERSION;

    char* argv[] = {
        "-x",
        _strdup(T2A((LPCTSTR)src)),
        "-d",
        _strdup(T2A((LPCTSTR)dst)),
    };

    int nret = main_unz(4, argv);
    free(argv[1]);
    free(argv[3]);

    return nret == 0;
}

#ifdef __cplusplus
};
#endif