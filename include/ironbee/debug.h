/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef _IB_DEBUG_H_
#define _IB_DEBUG_H_

/**
 * @file
 * @brief IronBee - Debugging Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/types.h>
#include <ironbee/array.h>
#include <ironbee/list.h>
#include <ironbee/field.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilDebug Debugging
 * @ingroup IronBeeUtil
 * @{
 */

#ifdef IB_DEBUG
/**
 * Initialize tracing system.
 *
 * @todo This will evolve into something much more useful.
 *
 * @param fn Filename to log tracing to (or NULL for default)
 */
void DLL_PUBLIC ib_trace_init(const char *fn);

/**
 * @internal
 * Log a generic trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 */
void DLL_PUBLIC ib_trace_msg(const char *file,
                             int line,
                             const char *func,
                             const char *msg);

/**
 * @internal
 * Log a numeric trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param num Number to log
 */
void DLL_PUBLIC ib_trace_num(const char *file,
                             int line,
                             const char *func,
                             const char *msg,
                             intmax_t num);

/**
 * @internal
 * Log a unsigned numeric trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param unum Unsigned number to log
 */
void DLL_PUBLIC ib_trace_unum(const char *file,
                              int line,
                              const char *func,
                              const char *msg,
                              uintmax_t unum);

/**
 * @internal
 * Log a pointer address trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param ptr Address to log
 */
void DLL_PUBLIC ib_trace_ptr(const char *file,
                             int line,
                             const char *func,
                             const char *msg,
                             void *ptr);

/**
 * @internal
 * Log a string trace message.
 *
 * @param file Source filename (typically __FILE__)
 * @param line Source line (typically __LINE__)
 * @param func Source function name
 * @param msg Message to log
 * @param str String to log
 */
void DLL_PUBLIC ib_trace_str(const char *file,
                             int line,
                             const char *func,
                             const char *msg,
                             const char *str);

/**
 * Initialize function tracing for a function.
 *
 * This should be the first line of a function and is required before
 * any other ftrace macro is used.
 *
 * @code
 * ib_status_t my_func_name(int foo)
 * {
 *     IB_FTRACE_INIT(my_func_name);
 *     ...
 *     IB_FTRACE_RET_STATUS(IB_OK);
 * }
 * @endcode
 *
 * @param name Name of function
 */
#define IB_FTRACE_INIT(name) \
    const char *__ib_fname__ = IB_XSTRINGIFY(name); \
    ib_trace_msg(__FILE__, __LINE__, __ib_fname__, "called")
    
/**
 * Logs a string message to the ftrace log.
 *
 * @param msg String message
 */
#define IB_FTRACE_MSG(msg) \
    ib_trace_msg(__FILE__, __LINE__, __ib_fname__, (msg))

/**
 * Return wrapper for functions which do not return a value.
 */
#define IB_FTRACE_RET_VOID() \
    ib_trace_msg(__FILE__, __LINE__, __ib_fname__, "returned"); \
    return

/**
 * Return wrapper for functions which return a @ref ib_status_t value.
 *
 * @param rv Return value
 */
#define IB_FTRACE_RET_STATUS(rv) \
    do { \
        ib_status_t __ib_ft_rv = rv; \
        if (__ib_ft_rv != IB_OK) { \
            ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned error", (intmax_t)__ib_ft_rv); \
        } \
        else { \
            ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned success", (intmax_t)__ib_ft_rv); \
        } \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return an int value.
 *
 * @param rv Return value
 */
#define IB_FTRACE_RET_INT(rv) \
    do { \
        int __ib_ft_rv = rv; \
        ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned", (intmax_t)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a size_t value.
 *
 * @param rv Return value
 */
#define IB_FTRACE_RET_SIZET(rv) \
    do { \
        size_t __ib_ft_rv = rv; \
        ib_trace_num(__FILE__, __LINE__, __ib_fname__, "returned", (intmax_t)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a pointer value.
 *
 * @param type The type of pointer to return
 * @param rv Return value
 */
#define IB_FTRACE_RET_PTR(type,rv) \
    do { \
        type *__ib_ft_rv = rv; \
        ib_trace_ptr(__FILE__, __LINE__, __ib_fname__, "returned", (void *)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a string value.
 * 
 * @param rv Return value
 */
#define IB_FTRACE_RET_STR(rv) \
    do { \
        char *__ib_ft_rv = rv; \
        ib_trace_str(__FILE__, __LINE__, __ib_fname__, "returned", (const char *)__ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

/**
 * Return wrapper for functions which return a constant string value.
 * 
 * @param rv Return value
 */
#define IB_FTRACE_RET_CONSTSTR(rv) \
    do { \
        const char *__ib_ft_rv = rv; \
        ib_trace_str(__FILE__, __LINE__, __ib_fname__, "returned", __ib_ft_rv); \
        return __ib_ft_rv; \
    } while(0)

#else
#define ib_trace_init(fn)
#define ib_trace_msg(file,line,func,msg)
#define ib_trace_num(file,line,func,msg,num)
#define ib_trace_unum(file,line,func,msg,unum)
#define ib_trace_ptr(file,line,func,msg,ptr)
#define ib_trace_str(file,line,func,msg,str)

#define IB_FTRACE_INIT(name)
#define IB_FTRACE_MSG(msg)
#define IB_FTRACE_RET_VOID() return
#define IB_FTRACE_RET_STATUS(rv) return (rv)
#define IB_FTRACE_RET_INT(rv) return (rv)
#define IB_FTRACE_RET_UINT(rv) return (rv)
#define IB_FTRACE_RET_SIZET(rv) return (rv)
#define IB_FTRACE_RET_PTR(type,rv) return (rv)
#define IB_FTRACE_RET_STR(rv) return (rv)
#define IB_FTRACE_RET_CONSTSTR(rv) return (rv)
#endif /* IB_DEBUG */

/** @} IronBeeUtilDebug */


#ifdef __cplusplus
}
#endif

#endif /* _IB_DEBUG_H_ */