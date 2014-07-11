/**
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef AIOUSB_UTILS_H
#define AIOUSB_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>


#ifndef AIO_LOG_DOMAIN
#define AIO_LOG_DOMAIN ((char*) 0)
#endif


#if defined (__GNUC__) && defined (__cplusplus)
#define G_STRFUNC     ((const char*) (__PRETTY_FUNCTION__))
#elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define G_STRFUNC     ((const char*) (__func__))
#elif defined (__GNUC__) || (defined(_MSC_VER) && (_MSC_VER > 1300))
#define G_STRFUNC     ((const char*) (__FUNCTION__))
#else
#define G_STRFUNC     ((const char*) ("???"))
#endif

#ifdef AIOUSB_DISABLE_ASSERT
#define aio_assert(expr)   do { (void) 0; } while(0)
#else
#define aio_assert(expr)   do { if ( expr )  ; else \
            aio_assert_expr( AIO_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, #expr ); \
    } while(0) 
#endif

void aiousb_assertion_message(const char *domain,
                              const char     *file,
                              int           line,
                              const char     *func,
                              const char     *message);

void aio_assert_expr(const char *domain,
                     const char     *file,
                     int             line,
                     const char     *func,
                     const char     *expr);


extern char *__aio_assert_msg;

#endif
