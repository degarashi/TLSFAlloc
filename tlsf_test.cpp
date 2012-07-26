#include "tlsf.h"
using namespace rs;

int main() {
	const size_t bs = 1<<24-1;
	u8* buff = new u8[bs];
	TLSF<24,4,4,true> tls(buff, bs);
	tls.unit_test(1000);
    	return 0;
}
