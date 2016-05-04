#include <atlstr.h>
#include "../ANE_Zlib/zip_impl.h"



DWORD WINAPI extractThrd(void* p) {
    
    CString* psrc = (CString*)p;
    CString dst(_T("d:\\apk"));

    dst.AppendFormat(_T("_%d"), GetCurrentThreadId());

    UnZip(*psrc, dst);

    _tprintf(_T("%s extract finished."), (LPCTSTR)dst);
    return 0;
}

int _tmain(int argc, TCHAR* argv[]) {

    CString dst(_T("D:\\apk.zip"));
    CString src(_T("D:\\apk"));
#if 1
    Zip(src, dst);

    src.AppendFormat(_T("_2"));
    UnZip(dst, src);
#else

    HANDLE hThrd1 = CreateThread(NULL, 0, extractThrd, &dst, 0, NULL);
    HANDLE hThrd2 = CreateThread(NULL, 0, extractThrd, &dst, 0, NULL);

    HANDLE h[] = {hThrd1, hThrd2};
    WaitForMultipleObjects(2, h, TRUE, INFINITE);
    CloseHandle(hThrd1);
    CloseHandle(hThrd2);
#endif
    return 0;
}