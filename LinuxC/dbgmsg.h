//
//
#pragma once

//
//
#define Warn(msg)  fprintf(stderr, "#WARN %s: %s\n", __FUNCTION__, msg)
#define Error(msg)  fprintf(stderr, "*ERR %s: %s\n", __FUNCTION__, msg)
#define Error2(msg)  fprintf(stderr, "*ERR2 %s: %s=%s\n", __FUNCTION__, msg, arg)

