#ifndef __ZIP_H__
#define __ZIP_H__
#ifdef __cplusplus
extern "C" {
#endif

BOOL Zip(CString& src, CString& dst);
BOOL UnZip(CString& src, CString& dst);

#ifdef __cplusplus
};
#endif

#endif //__ZIP_H__