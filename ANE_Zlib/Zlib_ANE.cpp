#include <Windows.h>
#include <atlstr.h>
#include <FlashRuntimeExtensions.h>
#include "zip_impl.h"

extern CString g_err;


typedef
enum _OP_TYPE {
    OP_ZIP = 0,
    OP_UNZIP,
} OP_TYPE;

const TCHAR* getOpName(OP_TYPE op) {
    static const TCHAR* names[] = {
        _T("Zip"),
        _T("Unzip")
    };

    return names[op];
}


void ConvertUtf8ToGBK(CString& strgbk, const char* strutf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)strutf8, -1, NULL, 0);
    wchar_t* wszGBK = new wchar_t[len + 1];
    memset(wszGBK, 0, (len + 1)*sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)strutf8, -1, wszGBK, len);

    strgbk = wszGBK;
    delete[] wszGBK;
}


void ConvertGBKToUtf8(CString& strutf8, const TCHAR* strgbk) {
    //获取转换到UTF8编码后所需要的字符空间长度
    int nRetLen = ::WideCharToMultiByte(CP_UTF8, 0, strgbk, -1, NULL, 0, NULL, NULL);
    char* szUTF8 = new char[nRetLen + 3];
    memset(szUTF8, 0, nRetLen + 3);

    //转换到UTF8编码
    ::WideCharToMultiByte(CP_UTF8, 0, strgbk, -1, (char*)szUTF8, nRetLen, NULL, NULL);
    strutf8 = (TCHAR*)szUTF8;
    delete[] szUTF8;
}

typedef
struct _zipArgument {
    FREContext context;
    CString src;
    CString dst;
    OP_TYPE op;
} ZipArgument;

CString ZipOperationHelper(FREContext ctx, CString& src, CString& dst, OP_TYPE op, BOOL async,
                           BOOL realAsync);

DWORD WINAPI zipThread(LPVOID lpThreadParameter) {
    ZipArgument* args = reinterpret_cast<ZipArgument*>(lpThreadParameter);

    ZipOperationHelper(args->context, args->src, args->dst, args->op, FALSE, TRUE);

    delete args;
    return 0;
}



CString ZipOperationHelper(FREContext ctx, CString& src, CString& dst, OP_TYPE op, BOOL async,
                           BOOL realAsync) {

    if (async) {
        ZipArgument* args = new ZipArgument();
        args->context = ctx;
        args->src = src;
        args->dst = dst;
        args->op = op;
        HANDLE hThrd = CreateThread(NULL, 0, zipThread, args, NULL, NULL);

        if (hThrd) {
            CloseHandle(hThrd);
            return "TRUE";
        } else {
            delete args;
            return "FALSE";
        }
    } else {
        BOOL result = FALSE;

        switch (op) {
        case OP_ZIP: {
            result = Zip(src, dst);
        }
        break;

        case OP_UNZIP: {
            result = UnZip(src, dst);
        }
        break;

        default: {
            g_err = _T("invalid operation instruction");
        }
        break;
        }

        CString level;
        level.Format(_T("<op>%s</op>\n")
                     _T("<result>%s</result>\n")
                     _T("<src>%s</src>\n")
                     _T("<dst>%s</dst>\n")
                     _T("<msg>%s</msg>\n"),
                     getOpName(op),
                     result ? _T("true") : _T("false"),
                     (LPCTSTR)src,
                     (LPCTSTR)dst,
                     (LPCTSTR)g_err);

        CString levelutf8;
        ConvertGBKToUtf8(levelutf8, (LPCTSTR)level);

        if (realAsync) {
            FREDispatchStatusEventAsync(ctx, (const uint8_t*)"ZIP", (const uint8_t*)(LPCTSTR)levelutf8);
        }

        return levelutf8;
    }
}

#ifdef __cplusplus
extern "C" {
#endif


FREObject ZipFileOpEx(FREContext ctx,
                      uint32_t   argc,
                      FREObject  argv[],
                      OP_TYPE op,
                      BOOL async) {
    enum {
        ARG_STRING_SRC_ARGUMENT = 0,
        ARG_STRING_DST_ARGUMENT,
        ARG_COUNT
    };
    FREObject retObj = 0;

    _ASSERT(ARG_COUNT == argc);

    if (argc != ARG_COUNT) {
        g_err = _T("invalid argument number.");
        FRENewObjectFromBool(0, &retObj);
        return retObj;
    }

    // 解析出源文件路径，目标文件路径。
    // 需要从UTF8转换到GBK

    uint32_t strLength = 0;
    const uint8_t* nativeCharArray = NULL;

    FREResult status = FREGetObjectAsUTF8(argv[ ARG_STRING_SRC_ARGUMENT ], &strLength,
                                          &nativeCharArray);

    if (status != FRE_OK || strLength <= 0 || nativeCharArray == NULL) {
        g_err = _T("failed to extract source file path");
        FRENewObjectFromBool(0, &retObj);
        return retObj;
    }

    CString strSrc;
    ConvertUtf8ToGBK(strSrc, (LPCSTR)nativeCharArray);

    status = FREGetObjectAsUTF8(argv[ ARG_STRING_DST_ARGUMENT ], &strLength,
                                &nativeCharArray);

    if (status != FRE_OK || strLength <= 0 || nativeCharArray == NULL) {
        g_err = _T("failed to extract destination file path");
        FRENewObjectFromBool(0, &retObj);
        return retObj;
    }

    CString strDst;
    ConvertUtf8ToGBK(strDst, (LPCSTR)nativeCharArray);


    CString resultUtf8 = ZipOperationHelper(ctx, strSrc, strDst, op, async, FALSE);

    if (async) {
        BOOL result = (resultUtf8.CompareNoCase(_T("TRUE")) == 0);
        FRENewObjectFromBool(result, &retObj);
    } else {
        FRENewObjectFromUTF8(resultUtf8.GetLength()*sizeof(TCHAR), (const uint8_t*)((LPCTSTR)resultUtf8),
                             &retObj);
    }

    return retObj;
}


FREObject ZipFile(
    FREContext ctx,
    void*      functionData,
    uint32_t   argc,
    FREObject  argv[]) {
    return ZipFileOpEx(ctx, argc, argv, OP_ZIP, FALSE);
}

FREObject UnZipFile(
    FREContext ctx,
    void*      functionData,
    uint32_t   argc,
    FREObject  argv[]) {
    return ZipFileOpEx(ctx, argc, argv, OP_UNZIP, FALSE);
}

FREObject ZipFileAsynchrony(
    FREContext ctx,
    void*      functionData,
    uint32_t   argc,
    FREObject  argv[]) {
    return ZipFileOpEx(ctx, argc, argv, OP_ZIP, TRUE);
}

FREObject UnZipFileAsynchrony(
    FREContext ctx,
    void*      functionData,
    uint32_t   argc,
    FREObject  argv[]) {
    return ZipFileOpEx(ctx, argc, argv, OP_UNZIP, TRUE);
}

FREObject ErrorString(
    FREContext ctx,
    void*      functionData,
    uint32_t   argc,
    FREObject  argv[]) {
    USES_CONVERSION;

    FREObject retObj = NULL;
    FRENewObjectFromUTF8(g_err.GetLength(), (const uint8_t*)(T2A((LPCTSTR)g_err)), &retObj);
    return retObj;
}

void contextInitializer(
    void*                      extData,
    const uint8_t*              ctxType,
    FREContext                   ctx,
    uint32_t*                  numFunctionsToSet,
    const FRENamedFunction**     functionsToSet) {
    // Create mapping between function names and pointers in an array of FRENamedFunction.
    // These are the functions that you will call from ActionScript -
    // effectively the interface of your native library.
    // Each member of the array contains the following information:
    // { function name as it will be called from ActionScript,
    //   any data that should be passed to the function,
    //   a pointer to the implementation of the function in the native library }
    static FRENamedFunction extensionFunctions[] = {

        { (const uint8_t*) "ZipFile", NULL, &ZipFile},
        { (const uint8_t*) "UnZipFile", NULL, &UnZipFile},
        { (const uint8_t*) "ZipFileAsynchrony", NULL, &ZipFileAsynchrony},
        { (const uint8_t*) "UnZipFileAsynchrony", NULL, &UnZipFileAsynchrony},
        { (const uint8_t*) "ErrorString", NULL, &ErrorString }
    };

    // Tell AIR how many functions there are in the array:
    *numFunctionsToSet = sizeof(extensionFunctions) / sizeof(FRENamedFunction);

    // Set the output parameter to point to the array we filled in:
    *functionsToSet = extensionFunctions;
}


void contextFinalizer(FREContext ctx) {
    return;
}

void ExtensionInitializer(void** extData, FREContextInitializer* ctxInitializer,
                          FREContextFinalizer* ctxFinalizer) {
    // The name of function that will initialize the extension context
    *ctxInitializer = &contextInitializer;
    // The name of function that will finalize the extension context
    *ctxFinalizer = &contextFinalizer;
}

void ExtensionFinalizer(void* extData) {
    return;
}

#ifdef __cplusplus
};
#endif