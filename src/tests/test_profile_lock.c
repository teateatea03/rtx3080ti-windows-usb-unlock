/* Modified 2026-10-25: PNY/B550 manual USB port; see docs/CHANGES-PNY.md. */ /* An arbitrary armed flag must not authorize experimental operations. */
#define main port_fixture_main
#include "test_port.c"
#undef main
#ifdef GU_ENTRY_LATCHED_SS1_FULL
#define SS0 0x82381cU
#define SS1 0x823820U
static UINTN ss1_callbacks;
static BOOLEAN reenter_ss1,fail_ss1;
static EFI_STATUS EFIAPI selector_write(EFI_PCI_IO_PROTOCOL *p,
 EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 b,UINT64 o,UINTN n,VOID *v) {
  if(o==SS1) {
    ss1_callbacks++;
    if(reenter_ss1) {
      reenter_ss1=FALSE;
      GuWrite32(SS1,8); /* Reenter before the outer callback performs its write. */
    }
  }
  EFI_STATUS status=writemem(p,w,b,o,n,v);
  /* Model an error after a hardware effect as well as before one. */
  return o==SS1 && fail_ss1?EFI_DEVICE_ERROR:status;
}
static void selector_setup(UINT32 phase,UINT32 entry1) {
  setup();ss1_callbacks=0;reenter_ss1=fail_ss1=FALSE;
  pci.Mem.Write=selector_write;Gu.prom_verified=TRUE;
  registers[SS0/4]=0x03000062;registers[SS1/4]=entry1;
  assert(GuLatchEntrySelectors(registers[SS0/4],entry1));
  assert(!EFI_ERROR(GuSavePci()));
  if(phase>=GU_SELECTOR_PHASE_SS0_WRITTEN) {
    GuWrite32(SS0,0x88888888);
    assert(GuSelectorPhase(GU_SELECTOR_PHASE_SS0_WRITTEN));
  }
  if(phase>=GU_SELECTOR_PHASE_SS1_WRITE)
    assert(GuSelectorPhase(GU_SELECTOR_PHASE_SS1_WRITE));
  if(phase>=GU_SELECTOR_PHASE_TARGET) {
    GuWrite32(SS1,8);assert(!Gu.fault && ss1_callbacks==1);
    assert(GuSelectorPhase(GU_SELECTOR_PHASE_TARGET));
  }
}
static void fault_containment(void) {
  assert(Gu.fault);UINTN writes=fake.writes;
  GuWrite32(SS1,8);assert(fake.writes==writes);
  assert(EFI_ERROR(GuFlr()) && !fake.flr_seen);
  config[4]|=4;
  assert(!EFI_ERROR(GuClearMaster()) && !(config[4]&4) && Gu.fault);
}
static void selector_regressions(void) {
  setup();Gu.profile_verified=FALSE;Gu.armed=Gu.owned=FALSE;
  assert(!GuSelectorPhase(0xffffffffU) && Gu.fault && !fake.reads && !fake.writes);
  const UINT32 phases[]={GU_SELECTOR_PHASE_ENTRY,GU_SELECTOR_PHASE_SS0_WRITTEN,
    GU_SELECTOR_PHASE_SS1_WRITE,GU_SELECTOR_PHASE_TARGET,4,0xffffffffU};
  /* Exhaust the whitelist from every reachable phase: no setter/reset,
     same-phase, skipped phase, backward edge or unknown input is accepted. */
  for(UINT32 current=GU_SELECTOR_PHASE_ENTRY;current<=GU_SELECTOR_PHASE_TARGET;current++) {
    for(UINTN i=0;i<sizeof(phases)/sizeof(phases[0]);i++) {
      UINT32 next=phases[i];selector_setup(current,6);
      if(next==current+1 && next<=GU_SELECTOR_PHASE_TARGET) {
        if(next==GU_SELECTOR_PHASE_SS0_WRITTEN)GuWrite32(SS0,0x88888888);
        if(next==GU_SELECTOR_PHASE_TARGET)GuWrite32(SS1,8);
        assert(GuSelectorPhase(next) && !Gu.fault);
      } else {
        UINTN reads=fake.reads,writes=fake.writes;
        assert(!GuSelectorPhase(next) && Gu.fault);
        assert(fake.reads==reads && fake.writes==writes);
        fault_containment();
      }
    }
  }
  puts("PASS phase whitelist: same-phase SS1_WRITE, ENTRY reset, unknown, skip and backtracking latch fault");

  /* A target-looking SS1 (including entry SS1=8) is not evidence of our write. */
  for(UINT32 entry1=6;entry1<=8;entry1++) {
    selector_setup(GU_SELECTOR_PHASE_SS1_WRITE,entry1);registers[SS1/4]=8;
    UINTN reads=fake.reads;
    assert(!GuSelectorPhase(GU_SELECTOR_PHASE_TARGET) && Gu.fault);
    assert(fake.reads==reads && !ss1_callbacks);fault_containment();
  }
  puts("PASS TARGET requires consumed SS1 write before register comparison, including entry SS1=8");

  selector_setup(GU_SELECTOR_PHASE_SS1_WRITE,7);reenter_ss1=TRUE;
  GuWrite32(SS1,8);
  assert(ss1_callbacks==1 && fake.writes==2 && Gu.mmio_writes==2 && Gu.fault);
  assert(registers[SS1/4]==8);fault_containment();
  puts("PASS external Mem.Write callback reentrant duplicate rejected before a second callback");

  for(unsigned after_effect=0;after_effect<2;after_effect++) {
    selector_setup(GU_SELECTOR_PHASE_SS1_WRITE,7);
    fake.mem_fail=!after_effect;fail_ss1=TRUE;GuWrite32(SS1,8);
    assert(ss1_callbacks==1 && Gu.fault && fake.writes==2);
    assert(registers[SS1/4]==(after_effect?8U:7U));fault_containment();
    /* Test-only fault removal isolates the private spent token: fault alone
       must not be the reason retry is rejected. Production never does this. */
    Gu.fault=FALSE;fake.mem_fail=0;fail_ss1=FALSE;GuWrite32(SS1,8);
    assert(Gu.fault && ss1_callbacks==1 && fake.writes==2);
  }
  puts("PASS failed SS1 write stays consumed with and without hardware effect; retry blocked");

  selector_setup(GU_SELECTOR_PHASE_SS1_WRITE,7);GuWrite32(SS1,8);
  assert(!Gu.fault && ss1_callbacks==1);
  Gu.armed=FALSE;Gu.armed=TRUE;GuWrite32(SS1,8);
  assert(Gu.fault && ss1_callbacks==1);fault_containment();
  selector_setup(GU_SELECTOR_PHASE_SS1_WRITE,7);GuWrite32(SS1,8);
  assert(!GuSelectorPhase(GU_SELECTOR_PHASE_SS1_WRITE) && Gu.fault && ss1_callbacks==1);
  fault_containment();
  selector_setup(GU_SELECTOR_PHASE_TARGET,7);GuWrite32(SS1,8);
  assert(Gu.fault && ss1_callbacks==1);fault_containment();
  /* Only a new GuInit invocation, followed by a fresh latch, gets a new token. */
  for(UINT32 entry1=6;entry1<=8;entry1++) {
    selector_setup(GU_SELECTOR_PHASE_TARGET,entry1);
    assert(!Gu.fault && ss1_callbacks==1 && !EFI_ERROR(GuFlr()) && fake.flr_seen);
  }
  puts("PASS repeated write/phase and armed-toggle cannot rearm; fresh GuInit valid path and FLR preserved");
}
#endif
int main(void) {
  VOID *cpu;UINT64 dma;UINT32 value=4;
  setup();Gu.profile_verified=FALSE;
  assert(EFI_ERROR(GuDmaAllocate(4096,&cpu,&dma)) && !fake.maps && !fake.allocs);
  setup();Gu.profile_verified=FALSE;GuWrite32(0,1);
  assert(!fake.writes && Gu.fault);
  setup();Gu.profile_verified=FALSE;
  assert(EFI_ERROR(GuConfigWrite(EfiPciIoWidthUint32,4,&value)) && !fake.pciwrites);
  setup();Gu.profile_verified=FALSE;assert(EFI_ERROR(GuEnableMaster()) && !fake.pciwrites);
  setup();Gu.profile_verified=FALSE;Gu.saved=TRUE;
  assert(EFI_ERROR(GuFlr()) && !fake.flr_seen && !fake.pciwrites);
  setup();Gu.profile_verified=FALSE;Gu.fault=TRUE;config[4]|=4;
  assert(!EFI_ERROR(GuClearMaster()) && !(config[4]&4));
  puts("PASS experimental armed-without-profile rejection and fault containment");
#ifdef GU_ENTRY_LATCHED_SS1_FULL
  selector_regressions();
#elif defined(GU_ENTRY_LATCHED_SS1_PRESERVE)
  setup();GuWrite32(0x823820,8);assert(Gu.fault && !fake.writes);
  puts("PASS preserve policy rejects every SS1 write");
#else
  setup();GuWrite32(0x823820,8);GuWrite32(0x823820,8);
  assert(!Gu.fault && fake.writes==2 && registers[0x823820/4]==8);
  puts("PASS original/observed SS1 policy preserved");
#endif
  return 0;
}
