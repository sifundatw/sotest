# sotest self-test script
use test.so
call example
call test1_func
use other.so
call func1
; syntax errors only warn:
bogus command
call
use other.so extra
call \
   example
call missing_func    # execution error: terminates the script
call func1           # never reached
