// This file is a modeling file for coverity

namespace lyx {

// Tell coverity that this function exits when value is false
void doAssertWithCallstack(bool value)
{
	if (!value) {
		 __coverity_panic__();
	}
}


void doAssertStatic(char const * expr, char const * file, long line)
{
	__coverity_panic__();
}


// Tell coverity that this function always exits
void doAppErr(char const * expr, char const * file, long line)
{
	__coverity_panic__();
}


void lyx_exit(int exit_code)
{
	__coverity_panic__();
}


void lyxbreaker(void const * data, const char * hint, int size)
{
	__coverity_panic__();
}


}
