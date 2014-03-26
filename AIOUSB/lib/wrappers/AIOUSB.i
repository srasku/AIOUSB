
%module AIOUSB
%include "cpointer.i"

%pointer_functions( unsigned long,  ulp );
%pointer_functions( unsigned short, usp );
%pointer_functions( double , dp );
/* %array_functions(unsigned short *, ushort ); */

%include typemaps.i
%apply unsigned long *INOUT { unsigned long *result };
%{
/*   extern unsigned long AIOUSB_GetStreamingBlockSize(unsigned long DeviceIndex, unsigned long *BlockSize ); */
  extern unsigned long ADC_BulkPoll( unsigned long DeviceIndex, unsigned long *INOUT );
%}


%{
  #include "aiousb.h"
  #include "AIOUSB_Core.h"
  #include "libusb.h"
  #include "AIOContinuousBuffer.h"
  #include "AIOTypes.h"
%}

/* Needed to allow inclusion into Scala */
%pragma(java) modulecode=%{
    static {
        System.loadLibrary("AIOUSB"); 
    }
%}

%newobject CreateSmartBuffer;
%newobject NewBuffer;
%delobject AIOBuf::DeleteBuffer;

%include "aiousb.h"
%include "AIOUSB_Core.h"
%include "AIOContinuousBuffer.h"
%include "AIOTypes.h"



%inline %{
  unsigned short *new_ushortarray(int size) {
      return (unsigned short *)malloc(size*sizeof(unsigned short));
  }

  void delete_ushortarray( unsigned short *ary ) {
    free(ary);
  }

  int ushort_getitem(unsigned short *ary, int index) {
    return (int)ary[index];
  }

  void ushort_setitem( unsigned short *ary, int index, int value ) {
    ary[index] = (unsigned short)value;
  }
  /* unsigned short *ushort_array( int size ) { */
  /*   return (unsigned short *)malloc(size*sizeof(unsigned short)); */
  /* } */
  /* int ushort_array_get( unsigned short *ary, int index ) { */
  /*   return (int)ary[index]; */
  /* } */

%}

/* %extend Foo { */
/*   Foo( int bufsize ) { */
/*     struct Foo *tmp = (struct Foo *)malloc(sizeof(struct Foo)); */
/*     tmp->ary = (unsigned short *)malloc(sizeof(unsigned short)*bufsize); */
/*     return tmp; */
/*   } */
/* } */

%extend AIOContinuousBuf {

  AIOContinuousBuf( unsigned long deviceIndex, unsigned numScans, unsigned numChannels ) {
    return (AIOContinuousBuf *)NewAIOContinuousBufForCounts( deviceIndex, numScans, numChannels );
  }

  ~AIOContinuousBuf() {
    DeleteAIOContinuousBuf($self);
  }
}

%extend AIOBuf {
  AIOBuf(int bufsize)  {
    return (AIOBuf *)NewBuffer( bufsize );
  }
  ~AIOBuf()  {
    DeleteBuffer($self);
  }

}




