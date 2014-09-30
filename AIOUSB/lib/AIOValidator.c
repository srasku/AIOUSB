#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "AIOValidator.h"

#ifdef __cplusplus
namespace AIOUSB {
#endif


VALIDATOR_INTERNAL_API(struct validator);

/*----------------------------------------------------------------------------*/
AIORET_TYPE ValidateChain( Validator *self ) 
{
    AIORET_TYPE result = 0;
    for ( Validator *cur = self; cur ; cur = cur->next ) {
        if ( cur->Validate ) {  /* We can ignore blank chains */
            AIORET_TYPE tmpresult = cur->Validate( self );
            result |= tmpresult;
            if ( result < 0 ) 
                break;
        }
    }

    return result;
}

/*----------------------------------------------------------------------------*/
void DeleteValidators( Validator *tmp ) 
{
    Validator *cur = tmp;
    Validator *next = NULL;
    for( cur = tmp; cur; cur = next ) {
        if ( !cur )
            break;
        next = cur->next;
        free(cur);
    }
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE NumberValidators( Validator  *self )
{
    if ( !self ) { 
        return AIOUSB_ERROR_INVALID_DATA;
    }
    int count = 0;
    Validator *cur;
    for (cur = self; cur; cur = cur->next) {
        if ( cur->Validate )
            count ++;
    }

    return count;
}

Validator *ExternalNewValidator( AIORET_TYPE (*validate_fn)( Validator *obj ) )
{
    Validator *tmp = (Validator*)calloc(sizeof(Validator),1);
    if ( !tmp )
        return tmp;
    tmp->Validate          = validate_fn;
    tmp->ValidateChain     = ValidateChain;
    tmp->NumberValidators  = NumberValidators;
    tmp->DeleteValidators  = DeleteValidators;
    tmp->AddValidator      = AddValidator;
    return tmp;
}

Validator *SetupValidator( Validator *tmp)
{
    if ( !tmp )
        return tmp;
    tmp->ValidateChain     = ValidateChain;
    tmp->NumberValidators  = NumberValidators;
    tmp->DeleteValidators  = DeleteValidators;
    tmp->AddValidator      = AddValidator;
    return tmp;    
}

/*----------------------------------------------------------------------------*/
static Validator *NewValidator( AIORET_TYPE (*validate_fn)( Validator *obj ) ) 
{
    Validator *tmp = (Validator*)calloc(sizeof(Validator),1);
    /* tmp->Validate  = (VALIDATOR_FN)validate_fn; */
    tmp->Validate  = validate_fn;
    return SetupValidator( tmp );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AddValidator( Validator *self, Validator *next )
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( !next ) 
        return AIOUSB_ERROR_INVALID_DATA;
    if ( !self )  {
        /* *self = next; */
        return AIOUSB_ERROR_INVALID_DATA;
    }
    Validator *cur;
    for ( cur = self; cur->next ; cur = cur->next );
    
    cur->next = next;

    return result;
}

#ifdef __cplusplus
}
#endif


#ifdef SELF_TEST

#include "gtest/gtest.h"
#include "tap.h"
#include "AIOValidator.h"

using namespace AIOUSB;

/*----------------------------------------------------------------------------*/
class ValidateSetup : public ::testing::Test 
{
 protected:
    virtual void SetUp() {
        curvalue = 0;
        memset(buf,0,BUFSIZE );
        top = NewValidator(NULL);
    }
  
    virtual void TearDown() { 
    }
 public:
    static const int BUFSIZE = 1024;
    static char buf[BUFSIZE];
    static int curvalue;
    Validator *top;
};

int ValidateSetup::curvalue;
char ValidateSetup::buf[BUFSIZE];

AIORET_TYPE check_test( Validator *obj )
{
    sprintf( ValidateSetup::buf,"%s%d", ValidateSetup::buf, ValidateSetup::curvalue );
    ValidateSetup::curvalue += 1;
}

AIORET_TYPE fails_on_the_last( Validator *obj )
{
    if ( ValidateSetup::curvalue++ >= 100 ) { 
        return -1;
    } else {
        return 0;
    }
}

TEST_F(ValidateSetup,CoreValidate )
{
    Validator *ttmp = NewValidator( check_test );
    AIORET_TYPE retval;
    top->AddValidator( top, ttmp );
    EXPECT_EQ( top->NumberValidators(top), 1 ); /* Start with 1 */
    for ( int i = 0; i < 100 ; i ++ ) { 
        top->AddValidator( top, NewValidator( check_test )); /* Now add 100 more */
    } 
    EXPECT_EQ( top->NumberValidators(top), 101 );

    retval = top->ValidateChain( top );
    EXPECT_STREQ("0123456789101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899100", buf );
    EXPECT_GE( retval, 0 );

    DeleteValidators( top );
}

TEST_F(ValidateSetup, ShouldFailValidate )
{
    /* Validator *top = NULL; */
    Validator *ttmp = NewValidator( check_test );
    AIORET_TYPE retval;
    top->AddValidator( top, ttmp );
    for ( int i = 0; i < 101; i ++ ) { 
        top->AddValidator( top, NewValidator( fails_on_the_last ));
    }
    EXPECT_EQ(top->NumberValidators(top), 102 );
    retval = top->ValidateChain( top );
    EXPECT_LE( retval, -1 );
    DeleteValidators( top );

}

int
main(int argc, char *argv[] ) 
{
    testing::InitGoogleTest(&argc, argv);
    testing::TestEventListeners & listeners = testing::UnitTest::GetInstance()->listeners();
#ifdef GTEST_TAP_PRINT_TO_STDOUT
    delete listeners.Release(listeners.default_result_printer());
#endif
    /* listeners.Append( new tap::TapListener() ); */
   
    return RUN_ALL_TESTS();  
}


#endif
