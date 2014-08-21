#ifndef _VALIDATOR_H
#define _VALIDATOR_H

#include "AIOTypes.h"

#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif

/* #define SUCCESS                 0 */
/* #define INVALID_DATA           -1 */
/* typedef int AIORET_TYPE; */

/*----------------------------  PUBLIC INTERFACE  ----------------------------*/
#define VALIDATOR_INTERFACE(T)                                          \
    AIORET_TYPE (*Validate)( T*obj );                                   \
    AIORET_TYPE (*ValidateChain)( T*obj );                              \
    struct validator *SetupValidator( struct validator *tmp);           \
    AIORET_TYPE (*AddValidator)( T *self, struct validator *next );     \
    void (*DeleteValidators)( T*top );                                  \
    AIORET_TYPE (*NumberValidators)( T *self )                          \

/*------------------------  STRUCTURE DEFINITION   --------------------------*/
typedef struct validator {
    VALIDATOR_INTERFACE( struct validator );
    struct validator *next;
} Validator;

/*---------------------  INTERNAL API FOR VALIDATOR.C  ----------------------*/

#define VALIDATOR_INTERNAL_API(T)                                       \
    AIORET_TYPE ValidateChain( T*self );                                \
    void DeleteValidators( T*tmp );                                     \
    Validator *SetupValidator( Validator *tmp);                         \
    AIORET_TYPE NumberValidators( T *self );                            \
    static T*NewValidator( AIORET_TYPE (*validate_fn)( T*obj ) );       \
    AIORET_TYPE AddValidator( T*self, Validator*next );                 \
    T*ExternalNewValidator( AIORET_TYPE (*validate_fn)( T*obj ) );

typedef int (*VALIDATOR_FN)(validator*) ;

/*----------------  API EXTPORTED TO A CLASS OF TYPE (T)  ------------------*/

#define VALIDATOR_API(T)                                                \
    Validator *SetupValidator( Validator *tmp);                         \
    AIORET_TYPE ValidateChain( T*self ) {                               \
        self->validator.ValidateChain( &self->validator );              \
    }                                                                   \
    void DeleteValidators( T*self ) {                                   \
        self->validator.DeleteValidators( self->validator.next );      \
    }                                                                   \
    AIORET_TYPE NumberValidators( T *self ) {                           \
        self->validator.NumberValidators( &self->validator );           \
    }                                                                   \
    Validator*ExternalNewValidator( AIORET_TYPE (*validate_fn)( Validator*obj ) ); \
    AIORET_TYPE AddValidator( T*self, Validator*next ) {                        \
        self->validator.AddValidator( &self->validator, next ); \
    }                                                                   \
    Validator*NewValidator( AIORET_TYPE (*validate_fn)( T*obj ) ) {     \
        return ExternalNewValidator( (VALIDATOR_FN)validate_fn );       \
    }


/*------------  ELEMENT THAT MUST ME INCLUDED IN OTHER OBJECTS  -------------*/
#define VALIDATOR_MIXIN()   Validator validator;

/*--------------------  REQUIRED CONSTRUCTOR ARGUMENT  ----------------------*/

#define MIXIN_VALIDATOR_ALLOCATOR( tmp, Validator )  SetupValidator( &tmp->validator ); \
    tmp->NumberValidators = NumberValidators;\
    tmp->DeleteValidators = DeleteValidators;\
    tmp->ValidateChain = ValidateChain;\
    tmp->AddValidator = AddValidator;


#ifdef __aiousb_cplusplus
}
#endif

#endif
