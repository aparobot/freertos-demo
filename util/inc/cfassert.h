#ifndef __CFASSERT_H__
#define __CFASSERT_H__

#define ASSERT(e)  if (e) ; \
        else assertFail( #e, __FILE__, __LINE__ )


#define JOIN(x, y) XJOIN(x, y)
#define XJOIN(x, y) x ## y
#define static_assert(e) typedef char JOIN(assertion_failed_at_line_, __LINE__) [(e) ? 1 : -1]

/**
 * Assert handler function
 */
void assertFail(char *exp, char *file, int line);

#endif //__CFASSERT_H__
