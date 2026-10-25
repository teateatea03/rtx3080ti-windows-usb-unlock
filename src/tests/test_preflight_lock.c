/* Modified 2026-10-25: PNY/B550 manual USB port; see docs/CHANGES-PNY.md. */ #define main legacy_port_main
#include "test_port.c"
#undef main
int main(void) {
  VOID *cpu=NULL;UINT64 dma=0;UINT32 value=4;
  setup();Gu.armed=Gu.owned=Gu.display_ok=TRUE;
  assert(EFI_ERROR(GuDmaAllocate(4096,&cpu,&dma)) && !cpu && !dma && !fake.allocs);
  GuWrite32(0,1);assert(Gu.fault && !fake.writes);
  setup();assert(EFI_ERROR(GuConfigWrite(EfiPciIoWidthUint32,4,&value)) && !fake.pciwrites);
  setup();assert(EFI_ERROR(GuEnableMaster()) && !fake.pciwrites);
  setup();config[4]|=4;assert(EFI_ERROR(GuClearMaster()) && !fake.pciwrites);
  setup();Gu.saved=TRUE;assert(EFI_ERROR(GuFlr()) && !fake.flr_seen && !fake.pciwrites);
  puts("PASS production preflight write/DMA/master/FLR locks even with armed flag forced");return 0;
}
