#ifndef AIOUSB_LOG
#define AIOUSB_LOG

#include <pthread.h>
#include <stdio.h>

#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif

#define GREEN "\033[0;32m"
#define RED "\031[0;32m"
#define MAGENTA "\031[0;35m"
#define CYAN "\031[0;36m"

#define AIO_DEVEL_STR GREEN"<Devel>"
#define AIO_DEBUG_STR GREEN"<Debug>"
#define AIO_WARN_STR  CYAN"<Warn>"
#define AIO_INFO_STR  "<Info>"
#define AIO_ERROR_STR RED"<Error>"
#define AIO_FATAL_STR MAGENTA"<Fatal>"
#define AIO_RESET_STR "\033[0m"

#undef AIOUSB_LOG
#define AIOUSB_LOG(fmt, ... ) do {                                      \
        pthread_mutex_lock( &message_lock );                            \
    fprintf( (!outfile ? stdout : outfile ), fmt "\033[0m" ,  ##__VA_ARGS__ ); \
    pthread_mutex_unlock(&message_lock);                                \
  } while ( 0 )

#undef AIOUSB_DEVEL
#undef AIOUSB_DEBUG
#undef AIOUSB_WARN 
#undef AIOUSB_ERROR
#undef AIOUSB_FATAL 

typedef enum aio_error_levels { 
    AIOFATAL_LEVEL = 1,
    AIOERROR_LEVEL = 1,
    AIOINFO_LEVEL = 2,
    AIOWARN_LEVEL = 4,
    AIODEBUG_LEVEL = 8,
    AIODEVEL_LEVEL = 16,
} AIO_DEBUG_LEVEL;

extern AIO_DEBUG_LEVEL AIOUSB_DEBUG_LEVEL;




#ifdef AIOUSB_DEBUG_LOG
/**
 * To use logging messages, compile with -DAIOUSB_DEBUG_LOG
 **/

#define AIOUSB_TAP(x,...)  if( 1 ) { AIOUSB_LOG( ( x ? "ok -" : "not ok" ) __VA_ARGS__ ); }

#define AIOUSB_DEVEL(...)  do { if ( AIOUSB_DEBUG_LEVEL & AIODEVEL_LEVEL ) { AIOUSB_LOG(AIO_DEVEL_STR __VA_ARGS__ ); } } while(0)
#define AIOUSB_DEBUG(...)  do { if ( AIOUSB_DEBUG_LEVEL & AIODEBUG_LEVEL ) { AIOUSB_LOG(AIO_DEBUG_STR __VA_ARGS__ ); } } while(0)
#define AIOUSB_WARN(...)   do { if ( AIOUSB_DEBUG_LEVEL & AIOWARN_LEVEL )  { AIOUSB_LOG(AIO_WARN_STR  __VA_ARGS__ ); } } while(0)
#define AIOUSB_INFO(...)   do { if ( AIOUSB_DEBUG_LEVEL & AIOINFO_LEVEL )  { AIOUSB_LOG(AIO_INFO_STR  __VA_ARGS__ ); } } while(0)
#define AIOUSB_ERROR(...)  do { if ( AIOUSB_DEBUG_LEVEL & AIOERROR_LEVEL ) { AIOUSB_LOG(AIO_ERROR_STR __VA_ARGS__ ); } } while(0)
#define AIOUSB_FATAL(...)  do { if ( AIOUSB_DEBUG_LEVEL & AIOFATAL_LEVEL ) { AIOUSB_LOG(AIO_FATAL_STR __VA_ARGS__ ); } } while(0)

#else

#define AIOUSB_DEVEL( ... ) if ( 0 ) { }
#define AIOUSB_DEBUG( ... ) if ( 0 ) { }
#define AIOUSB_WARN(...)    if ( 0 ) { AIOUSB_LOG("<Warn>\t"  __VA_ARGS__ ) }
#define AIOUSB_INFO(...)    if ( 0 ) { AIOUSB_LOG("<Info>\t"  __VA_ARGS__ ); }
#define AIOUSB_ERROR(...)  AIOUSB_LOG("<Error>\t" __VA_ARGS__ )
#define AIOUSB_FATAL(...)  AIOUSB_LOG("<Fatal>\t" __VA_ARGS__ )

#endif

/**
 * Compile with -DAIOUSB_DISABLE_LOG_MESSAGES 
 * if you don't wish to see these warning messages
 **/

#ifndef TAP_TEST
#define LOG(...) do {                           \
    pthread_mutex_lock( &message_lock );        \
    printf( __VA_ARGS__ );                      \
    pthread_mutex_unlock(&message_lock);        \
  } while ( 0 );
#else
#define LOG(...) do { } while (0); 
#endif

extern int LOG_LEVEL;
extern pthread_t cont_thread;
extern pthread_mutex_t message_lock;
extern FILE *outfile;

#ifdef __aiousb_cplusplus
}
#endif


#endif
