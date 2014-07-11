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

#ifdef __cplusplus
namespace AIOUSB {
#endif


static int test_in_subprocess = 0;
static int test_nonfatal_assertions = 0;
static FILE *aio_assert_fh = NULL;

char *__aio_assert_msg;

#ifdef SELF_TEST
#define ABORT() while(0) { }
#else
#define ABORT() abort()
#endif



void aio_assert_expr(const char *domain,
                     const char     *file,
                     int             line,
                     const char     *func,
                     const char     *expr)
{
    char *s;
    if( ! aio_assert_fh )
        aio_assert_fh = stdout;

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
        ABORT();
}

void aiousb_assertion_message(const char *domain,
                              const char     *file,
                              int           line,
                              const char     *func,
                              const char     *message)
{
    char lstr[32];
    char *s;
    if( ! aio_assert_fh )
        aio_assert_fh = stdout;

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
    fprintf(aio_assert_fh, "\n%s\n", s);
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
      ABORT();
}

#ifdef __cplusplus
}
#endif

#ifdef SELF_TEST

#include "gtest/gtest.h"
#include "tap.h"
#include <signal.h>

using namespace AIOUSB;


TEST(AIOUSB_Assert, BasicAssert) {
    aio_assert( 1 == 1 );
    aio_assert( strcmp("something","something") == 0 );
}

static void handler(int sig)
{
    return;
}

TEST( AIOUSB_Assert, FailedAssert) {
    struct sigaction sa;
    int tmpval;
    FILE *tmpfh;
    char *tmpname;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    EXPECT_NE( (tmpval = sigaction(SIGABRT, &sa, NULL)),-1 );
    tmpname = tempnam(NULL,"atest");
    /* printf("Name was %s\n",tmpname); */
    tmpfh = fopen(tmpname,"w+");
    EXPECT_TRUE( tmpfh );
    aio_assert_fh = tmpfh;
    /* Key expression */
    aio_assert( 1 == 2 );
    
    /* Now verify that it did write out correctly */
    fclose(tmpfh);
    tmpfh = fopen(tmpname,"r");
    EXPECT_TRUE( tmpfh );
    char *line= NULL;
    int size;
    int success = 0;
    while ( (int)getline( &line, (size_t*)&size, tmpfh ) >= 0 ) {
        if( strstr( line , " assertion failed: (1 == 2)" ) ) {
            success = 1;
        }
    }
    fclose(tmpfh);
    unlink(tmpname);
    EXPECT_TRUE( success );
}

int main(int argc, char *argv[] )
{
  AIORET_TYPE retval;
  int i,j,pos;
  test_nonfatal_assertions = 1;

  testing::InitGoogleTest(&argc, argv);
  testing::TestEventListeners & listeners = testing::UnitTest::GetInstance()->listeners();
#ifdef GTEST_TAP_PRINT_TO_STDOUT
  delete listeners.Release(listeners.default_result_printer());
#endif

  listeners.Append( new tap::TapListener() );
  return RUN_ALL_TESTS();  

}

#endif
