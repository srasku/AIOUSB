/**
 * @file   AIOUSB_Assert.c
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %t$
 * @brief  
 */
/**
 * @brief: The following code is borrowed and modified from glib.
 *
 * GLib testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik, Sven Herzberg
 *
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

#include "AIOUSB_Assert.h"

static int test_in_subprocess = 0;
static int test_nonfatal_assertions = 0;

char *__aio_assert_msg;

void aio_assert_expr(const char *domain,
                     const char     *file,
                     int             line,
                     const char     *func,
                     const char     *expr)
{
    char *s;
    if (!expr)
        s = strdup("code should not be reached");
    else { 
        s = (char *)malloc( strlen(expr) + 25 );
        snprintf(s,strlen(expr)+25,"assertion failed: (%s)", expr );
    }
    aiousb_assertion_message(domain, file, line, func, s);
    free(s);
    
    /* Normally g_assertion_message() won't return, but we need this for
     * when test_nonfatal_assertions is set, since
     * g_assertion_message_expr() is used for always-fatal assertions.
     */
    if ( test_in_subprocess )
        _exit(1);
    else 
        abort ();
}

void aiousb_assertion_message(const char *domain,
                              const char     *file,
                              int           line,
                              const char     *func,
                              const char     *message)
{
    char lstr[32];
    char *s;
    
    if (!message)
        message = "code should not be reached";
    snprintf(lstr, 32, "%d", line);
    asprintf( &s, "%s%s%s%s%s%s%s%s%s%s%s", 
              domain ? domain : "", 
              domain && domain[0] ? ":" : "",
              "ERROR:",
              file,
              ":",
              lstr,
              ":",
              func,
              func[0] ? ":" : "",
              " ",
              message 
              );
    /* s = strncat( domain ? domain : "", domain && domain[0] ? ":" : "", */
    /*              "ERROR:", file, ":", lstr, ":", */
    /*              func, func[0] ? ":" : "", */
    /*              " ", message, NULL); */
    
    printf("**\n%s\n", s);
    /* g_test_log (G_TEST_LOG_ERROR, s, NULL, 0, NULL); */

    if (test_nonfatal_assertions) {
        free(s);
        return;
    }
    
    /* store assertion message in global variable, so that it can be found in a
     * core dump */
    if (__aio_assert_msg != NULL)
        /* free the old one */
        free (__aio_assert_msg);

    __aio_assert_msg = (char*) malloc (strlen (s) + 1);
    strcpy(__aio_assert_msg, s);

    free(s);
    
  if (test_in_subprocess) {
      /* If this is a test case subprocess then it probably hit this
       * assertion on purpose, so just exit() rather than abort()ing,
       * to avoid triggering any system crash-reporting daemon.
       */
      _exit (1);
  } else
      abort ();
}

/* void aio_assertion_message_cmpnum(const char     *domain, */
/*                                   const char     *file, */
/*                                   int             line, */
/*                                   const char     *func, */
/*                                   const char     *expr, */
/*                                   long double     arg1, */
/*                                   const char     *cmp, */
/*                                   long double     arg2, */
/*                                   char            numtype) */
/* { */
/*     char *s = NULL; */

/*     switch (numtype) { */
/*     case 'i':   s = g_strdup_printf ("assertion failed (%s): (%" G_GINT64_MODIFIER "i %s %" G_GINT64_MODIFIER "i)", expr, (gint64) arg1, cmp, (gint64) arg2); break; */
/*     case 'x':   s = g_strdup_printf ("assertion failed (%s): (0x%08" G_GINT64_MODIFIER "x %s 0x%08" G_GINT64_MODIFIER "x)", expr, (guint64) arg1, cmp, (guint64) arg2); break; */
/*     case 'f':   s = g_strdup_printf ("assertion failed (%s): (%.9g %s %.9g)", expr, (double) arg1, cmp, (double) arg2); break; */
/*       /\* ideally use: floats=%.7g double=%.17g *\/ */
/*     } */
/*     aio_assertion_message (domain, file, line, func, s); */
/*     free(s); */
/* } */


