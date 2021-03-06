/*
 *  VS_String.h
 *  VectorStorm
 *
 *  Created by Trevor Powell on 17/05/07.
 *  Copyright 2007 Trevor Powell. All rights reserved.
 *
 */

#ifndef VS_STRING_H
#define VS_STRING_H
#include "Utils/fmt/printf.h"

typedef std::string vsString;

// vsString vsFormatString( const char* format, ... );
#define vsFormatString fmt::sprintf

vsString vsNumberString(int number);

vsString vsTrimWhitespace( const vsString& input );

#define STR vsFormatString

extern const vsString vsEmptyString;


#endif // VS_STRING_H

