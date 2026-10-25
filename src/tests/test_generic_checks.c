/* Modified 2026-10-25: PNY/B550 manual USB port; see docs/CHANGES-PNY.md. */ /* Existing fixture-independent checks; original capture-dependent suite retained. */
#define main original_checks_main
#include "test_checks.c"
#undef main
int main(void){
 TestIdentity();TestPowerAndPcie();TestCapabilities();TestPaths();
 if(Failures)return 1;
 printf("PASS %u original fixture-independent config/path assertions; original hardware capture not available.\n",Checks);
 return 0;
}
