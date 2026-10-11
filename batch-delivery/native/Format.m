#import "Format.h"
#include "IOSObjectArray.h"
#include "java/lang/Long.h"
#include "java/lang/Integer.h"
#include "java/lang/Double.h"
#include "java/lang/IllegalStateException.h"
#include <unicode/unum.h>

id RVBFormat(NSString *op,IOSObjectArray *a,BOOL *handled){
 *handled=[op isEqual:@"format.number"];if(!*handled)return nil;
 id value=IOSObjectArray_Get(a,0);NSString *locale=IOSObjectArray_Get(a,1),*style=IOSObjectArray_Get(a,2);
 UNumberFormatStyle kind=[style isEqual:@"compact_short"]?UNUM_DECIMAL_COMPACT_SHORT:[style isEqual:@"compact_long"]?UNUM_DECIMAL_COMPACT_LONG:[style isEqual:@"percent"]?UNUM_PERCENT:UNUM_DECIMAL;
 UErrorCode error=U_ZERO_ERROR;UNumberFormat *format=unum_open(kind,NULL,0,locale.UTF8String,NULL,&error);
 if(U_FAILURE(error))@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"ICU number formatter could not initialize"];
 @try{
  int min=[IOSObjectArray_Get(a,3) intValue],max=[IOSObjectArray_Get(a,4) intValue];
  if(min>=0)unum_setAttribute(format,UNUM_MIN_FRACTION_DIGITS,min);if(max>=0)unum_setAttribute(format,UNUM_MAX_FRACTION_DIGITS,max);
  IOSObjectArray *digits=IOSObjectArray_Get(a,5);
  if(digits){for(int i=0;i<10;i++){NSString *s=IOSObjectArray_Get(digits,i);UChar b[32];if(s.length>32)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Digit string is too long"];
   [s getCharacters:b range:NSMakeRange(0,s.length)];unum_setSymbol(format,i==0?UNUM_ZERO_DIGIT_SYMBOL:(UNumberFormatSymbol)(UNUM_ONE_DIGIT_SYMBOL+i-1),b,(int32_t)s.length,&error);}}
  UChar buffer[256];int32_t count;
  if([value isKindOfClass:JavaLangLong.class])count=unum_formatInt64(format,[value longLongValue],buffer,256,NULL,&error);
  else count=unum_formatDouble(format,[value doubleValue],buffer,256,NULL,&error);
  if(U_FAILURE(error)||count>256)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"ICU number formatting failed"];
  return [NSString stringWithCharacters:buffer length:count];
 }@finally{unum_close(format);}
}
