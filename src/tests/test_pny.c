/* Modified 2026-10-25: PNY/B550 manual USB port; see docs/CHANGES-PNY.md. */ /* GPL-2.0-only. Actual local app/core; ONLY firmware services and hardware are simulated. */
#define GU_PREFLIGHT_FIXTURE
#include "test_app.c"
#undef main
VOID GuLog(const char *format,...);
VOID GuLogText(const char *text);
#include "../app.c"
#ifndef GU_PROFILE_PNY_B550_3080TI
#error PNY machine profile required
#endif
enum pny_fault {
 P_OK,P_ID,P_BUS,P_COMMAND,P_BAR,P_EXTRA_GPU,P_GOP_TARGET,P_GOP_COUNT,
 P_DISPLAY_ID,P_DISPLAY_BAR,P_DISPLAY_COMMAND,P_TEXT_EDGE,P_TEXT_EXTRA,
 P_ATTR_UNSUPPORTED,P_ATTR_GET,P_ATTR_DIRTY,P_ATTR_WARN,P_ENABLE_NOOP,
 P_ENABLE_ERROR,P_ENABLE_EXTRA,P_ENABLE_CONFIG,P_DISABLE_ERROR,P_RESTORE_CONFIG,
 P_OWNER,P_OWNER_SELF_MISSING,P_OWNER_SELF_EXTRA,P_READ_ERROR,
 P_SS0,P_SS1,P_MEMORY,P_WARM,P_FUSE,P_DISPLAY_LATE,P_DISPLAY_FINAL,
 P_KEY_SS0,P_KEY_SS1,P_LOG_ENABLE,P_LOG_RESTORE,P_ATTR_GET_AFTER,P_COUNT
};
static const char *pny_names[]={
 "ok","target-id","target-bus","initial-command","bar-resource","extra-gpu",
 "target-gop","extra-gop","display-id","display-bar","display-command",
 "missing-text-edge","duplicate-text-edge","unsupported-memory","attributes-get",
 "dirty-cached-attributes","warning-status","enable-noop","enable-error-after-effect",
 "enable-extra-bme","enable-other-config","disable-error","restore-other-config",
 "driver-owner","missing-self-owner","extra-self-owner","read-error","entry-ss0",
 "entry-ss1","wrong-vram","warm-state","fwsec-fuse","late-display","final-display",
 "key-ss0","key-ss1","decode-intent-log","decode-restore-log","attributes-get-after"
};
static enum pny_fault pf;
static EFI_PCI_IO_PROTOCOL display_pci;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL physical_text;
static UINT8 display_config[256];
static UINTN memory_enables,memory_disables,display_reads;
static BOOLEAN pny_injected;
static const char *mutate_stage;
static UINT32 mutate_offset,mutate_mask;
static UINTN writes_at_mutation;
static const UINT8 target_dp[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,4,127,255,4,0};
static const UINT8 display_dp[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,8,127,255,4,0};
static const UINT8 graphics_dp[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,8,2,3,8,0,1,0,0,0,127,255,4,0};
static EFI_GUID text_guid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
static EFI_STATUS EFIAPI pny_read_config(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,
 UINT32 off,UINTN count,VOID *out) {
 if(p==&display_pci) {
  CHECK(w<=EfiPciIoWidthUint32 && off+((UINTN)1<<w)*count<=256);
  memcpy(out,display_config+off,((UINTN)1<<w)*count);display_reads++;return EFI_SUCCESS;
 }
 CHECK(p==&pci);
 if(pf==P_READ_ERROR && memory_enables && off==0x10)return EFI_DEVICE_ERROR;
 for(UINTN i=0;i<count;i++){
  EFI_STATUS s=read_config(p,w,off+(UINT32)(i*((UINTN)1<<w)),1,(UINT8 *)out+i*((UINTN)1<<w));
  if(EFI_ERROR(s))return s;
 }
 return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI pny_display_write(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,
 UINT32 off,UINTN n,VOID *out){(void)p;(void)w;(void)off;(void)n;(void)out;CHECK(0);return EFI_ACCESS_DENIED;}
static EFI_STATUS EFIAPI pny_location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){
 CHECK(p==&pci||p==&display_pci);*s=*d=*f=0;*b=p==&pci?(pf==P_BUS?5:4):8;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI pny_locate(EFI_LOCATE_SEARCH_TYPE search,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **h){
 CHECK(search==ByProtocol&&!key&&(guid(g,&pg)||guid(g,&gg)));
 *n=guid(g,&pg)?(pf==P_EXTRA_GPU||app_mode==APP_DUPLICATE?3:2):(pf==P_GOP_COUNT?2:1);
 CHECK(!EFI_ERROR(allocate_pool(EfiLoaderData,*n*sizeof(**h),(VOID **)h)));
 if(guid(g,&pg)){(*h)[0]=H(3);(*h)[1]=H(4);if(*n==3)(*h)[2]=H(7);}
 else {(*h)[0]=H(5);if(*n==2)(*h)[1]=H(6);}
 return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI pny_handle(EFI_HANDLE h,EFI_GUID *g,VOID **p){
 *p=NULL;
 if(guid(g,&pg)){if(h==H(3)||h==H(7))*p=&pci;else if(h==H(4))*p=&display_pci;}
 else if(guid(g,&gg)){if(h==H(5)||h==H(6))*p=&gops[0];}
 else if(guid(g,&text_guid)){if(h==H(5))*p=&physical_text;else if(h==H(6))*p=&conout;}
 else if(guid(g,&dg)){
  if(h==H(3)||h==H(7))*p=(VOID *)target_dp;
  else if(h==H(4))*p=(VOID *)display_dp;
  else if(h==H(5))*p=(VOID *)(pf==P_GOP_TARGET?target_dp:graphics_dp);
  else return handle(h,g,p);
 }else return handle(h,g,p);
 return *p?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI pny_info(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **e,UINTN *n){
 *e=NULL;*n=0;
 if(h==H(5)&&guid(g,&text_guid)){
  *n=pf==P_TEXT_EDGE?0:pf==P_TEXT_EXTRA?2:1;
  if(*n){
   CHECK(!EFI_ERROR(allocate_pool(EfiLoaderData,*n*sizeof(**e),(VOID **)e)));
   for(UINTN i=0;i<*n;i++)(*e)[i]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(8),H(6),EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER,1};
  }
  return EFI_SUCCESS;
 }
 CHECK(h==H(3)&&(guid(g,&pg)||guid(g,&dg))&&current_tpl>=TPL_CALLBACK);
 if(!guid(g,&pg))return EFI_SUCCESS;
 if(pf==P_OWNER||app_mode==APP_OWNER)*n=1;
 else if(exclusive && pf!=P_OWNER_SELF_MISSING)*n=pf==P_OWNER_SELF_EXTRA?2:1;
 if(*n){
  CHECK(!EFI_ERROR(allocate_pool(EfiLoaderData,*n*sizeof(**e),(VOID **)e)));
  for(UINTN i=0;i<*n;i++)(*e)[i]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){
   exclusive?H(1):H(8),exclusive?NULL:H(3),exclusive?EFI_OPEN_PROTOCOL_EXCLUSIVE:EFI_OPEN_PROTOCOL_BY_DRIVER,1};
 }
 return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI pny_bar(EFI_PCI_IO_PROTOCOL *p,UINT8 bar,UINT64 *supports,VOID **resources){
 EFI_STATUS s=bar_info(p,bar,supports,resources);
 EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=*resources;
 r->AddrRangeMin=GU_BAR0_BASE;r->AddrRangeMax=GU_BAR0_END;
 if(pf==P_BAR)r->AddrTranslationOffset=1;
 return s;
}
static EFI_STATUS EFIAPI pny_attributes(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,
 UINT64 mask,UINT64 *out){
 CHECK(p==&pci);
 if(op==EfiPciIoAttributeOperationSupported){
  CHECK(out&&!mask);*out=pf==P_ATTR_UNSUPPORTED?0:0x13763; /* Actual firmware Supported mask. */
  return pf==P_ATTR_WARN?1:EFI_SUCCESS;
 }
 if(op==EfiPciIoAttributeOperationGet){
  CHECK(out&&!mask);*out=fake.pci_attributes;
  return pf==P_ATTR_GET|| (pf==P_ATTR_GET_AFTER&&memory_enables&&!memory_disables)?EFI_DEVICE_ERROR:EFI_SUCCESS;
 }
 CHECK(exclusive&&Gu.owned&&!out);
 if(mask==EFI_PCI_IO_ATTRIBUTE_BUS_MASTER){
  CHECK(Gu.armed&&Gu.display_ok);
  if(app_mode==APP_MASTER_DISABLE_ERROR&&op==EfiPciIoAttributeOperationDisable){
   fake.master_disables++;return EFI_DEVICE_ERROR;
  }
  EFI_STATUS s=master_attributes(p,op,mask,out);
  return app_mode==APP_MASTER_ENABLE_ERROR&&op==EfiPciIoAttributeOperationEnable?EFI_DEVICE_ERROR:s;
 }
 CHECK(mask==EFI_PCI_IO_ATTRIBUTE_MEMORY&&!(config[4]&4));
 if(op==EfiPciIoAttributeOperationEnable){
  CHECK(!Gu.started&&!fake.maps&&!fake.mmio_writes&&memory_enables++==0);
  if(pf==P_ENABLE_NOOP)return EFI_SUCCESS;
  config[4]|=2;fake.pci_attributes|=mask;
  if(pf==P_ENABLE_EXTRA){config[4]|=4;fake.pci_attributes|=EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;}
  if(pf==P_ENABLE_CONFIG)config[0x10]^=0x10;
  return pf==P_ENABLE_ERROR?EFI_DEVICE_ERROR:EFI_SUCCESS;
 }
 CHECK(op==EfiPciIoAttributeOperationDisable&&memory_disables++==0);
 CHECK(GuSafeToReturn()&&!fake.live_maps&&!fake.live_buffers&&!Gu.allocations);
 if(pf==P_DISABLE_ERROR)return EFI_DEVICE_ERROR;
 config[4]&=(UINT8)~2U;fake.pci_attributes&=~mask;
 if(pf==P_RESTORE_CONFIG)config[0x10]^=0x10;
 return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI pny_report(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data){
 if(pf==P_LOG_ENABLE&&!strncmp(data,"PNY decode intent:",18))return EFI_DEVICE_ERROR;
 if(pf==P_LOG_RESTORE&&!strncmp(data,"PNY decode restored:",20))return EFI_DEVICE_ERROR;
 EFI_STATUS s=file_write(f,n,data);
 if(!pny_injected&&mutate_stage&&strstr(report_text,mutate_stage)){
  registers[mutate_offset/4]^=mutate_mask;pny_injected=TRUE;writes_at_mutation=fake.mmio_writes;
 }
 if(!pny_injected&&pf==P_DISPLAY_LATE&&strstr(report_text,"ARMED: one transaction")){
  modes[0].FrameBufferBase+=4096;pny_injected=TRUE;
 }
 if(!pny_injected&&pf==P_DISPLAY_FINAL&&strstr(report_text,"post-FLR SEC2 reset released")){
  modes[0].FrameBufferBase+=4096;pny_injected=TRUE;
 }
 return s;
}
static EFI_STATUS EFIAPI pny_key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *key){
 EFI_STATUS s=read_key(p,key);
 if(s==EFI_SUCCESS&&(pf==P_KEY_SS0||pf==P_KEY_SS1)){
  registers[(pf==P_KEY_SS0?SS0:SS1)/4]^=1;pny_injected=TRUE;
 }
 return s;
}
static VOID pny_setup(enum app_case mode,enum pny_fault fault){
#ifdef GU_ENTRY_LATCHED
 test_entry_ss0=GU_PNY_ENTRY_SS0_ALT;test_entry_ss1=GU_PNY_ENTRY_SS1;
#endif
 app_setup(mode);pf=fault;bound=FALSE;
 memory_enables=memory_disables=display_reads=writes_at_mutation=0;pny_injected=FALSE;mutate_stage=NULL;mutate_mask=1;
 memcpy(config,captured_config,256);fake.pci_attributes=fault==P_ATTR_DIRTY?EFI_PCI_IO_ATTRIBUTE_MEMORY:0;
 if(fault==P_ID)config[0x2c]^=1;
 if(fault==P_COMMAND)config[4]=2;
 if(mode==APP_CONFIG)config[0xc8]^=1;
 memset(&display_pci,0,sizeof(display_pci));memset(display_config,0,256);
 *(UINT32 *)display_config=0x2c0210de;*(UINT32 *)(display_config+4)=7;
 *(UINT32 *)(display_config+8)=0x030000a1;*(UINT32 *)(display_config+0x10)=0xe8000000;
 *(UINT32 *)(display_config+0x2c)=0x176219da;
 if(fault==P_DISPLAY_ID)display_config[0x2c]^=1;
 if(fault==P_DISPLAY_BAR)display_config[0x13]^=1;
 if(fault==P_DISPLAY_COMMAND)display_config[4]=3;
 display_pci.Pci.Read=pny_read_config;display_pci.Pci.Write=pny_display_write;display_pci.GetLocation=pny_location;
 bs.HandleProtocol=pny_handle;bs.LocateHandleBuffer=pny_locate;bs.OpenProtocolInformation=pny_info;
 pci.Pci.Read=pny_read_config;pci.GetLocation=pny_location;pci.GetBarAttributes=pny_bar;pci.Attributes=pny_attributes;
 files[3].protocol.Write=pny_report;conin.ReadKeyStroke=pny_key;st.StandardErrorHandle=st.ConsoleOutHandle;
 registers[0x1183a4/4]=GU_DEVICE_MEMORY_SIZE;registers[0x8241e0/4]=3; /* Actual RUN0003: version 2, signature offset 768. */
 registers[SS0/4]=GU_PNY_ENTRY_SS0_ALT;registers[SS1/4]=GU_PNY_ENTRY_SS1;
 if(fault==P_SS0)registers[SS0/4]^=1;
 if(fault==P_SS1)registers[SS1/4]=6;
 if(fault==P_MEMORY)registers[0x1183a4/4]^=1;
 if(fault==P_WARM)registers[0x118234/4]=0xff;
 if(fault==P_FUSE)registers[0x8241e0/4]=7; /* version 3 absent from mask 7 */
}
static EFI_STATUS pny_run(VOID){
 EFI_STATUS s=EFI_ABORTED;if(!setjmp(escape_halt))s=efi_main(H(1),&st);return s;
}
static VOID pny_common(VOID){
 CHECK(!disconnect_calls&&exclusive_calls<=1&&fake.gsp_starts<=1&&fake.sec2_starts<=1&&fake.flrs<=1);
 for(UINTN i=0;i<fake.trace_count;i++)CHECK(fake.write_trace[i][0]||fake.write_trace[i][2]!=SS1);
}
static VOID pny_clean(VOID){
 pny_common();CHECK(!halted&&!Gu.retained&&!Gu.allocations&&!fake.live_maps&&!fake.live_buffers&&!fake.pool_live);
 CHECK(!fake.pci_attributes&&!(*(UINT16 *)(config+4))&&!exclusive&&!Gu.master_attributes_pending);
 CHECK(!PnyDecodePending);
}
static VOID pny_no_transaction(VOID){
 CHECK(!Gu.started&&!fake.mmio_writes&&!fake.live_maps&&!fake.gsp_starts&&!fake.sec2_starts&&!fake.flrs);
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IONBF,0);CHECK(argc==4);load_fixture_assets(argv[1],argv[2]);
 memcpy(originals,GuAssets,sizeof(originals));size_t n;UINT8 *c=read_file(argv[3],&n);
 CHECK(n==256);memcpy(captured_config,c,n);free(c);
 CHECK(GuHashMatches(prom,sizeof(prom),GU_PROM_SHA256));
#ifndef GU_EXPERIMENTAL_3090
 (void)pny_names;
 pny_setup(APP_SUCCESS,P_OK);CHECK(pny_run()!=EFI_SUCCESS);pny_no_transaction();pny_clean();
 CHECK(!fake.maps&&!fake.pci_writes&&!memory_enables&&!memory_disables&&!exclusive_calls&&!key_calls);
 dispose_host_fixture();for(UINTN i=0;i<5;i++)free((VOID *)originals[i].firmware.data);
 puts("PNY default diagnostic: actual app rejected before any PCI/MMIO write, DMA, ownership or input PASS");return 0;
#else
 unsigned passed=0;
 /* Both measured stock baselines work; even transitions between them must not relatch. */
 const UINT32 measured[]={GU_PNY_ENTRY_SS0,GU_PNY_ENTRY_SS0_ALT};
 for(UINTN i=0;i<2;i++){
  pny_setup(APP_SUCCESS,P_OK);registers[SS0/4]=measured[i];
  CHECK(pny_run()==EFI_SUCCESS);pny_clean();CHECK(registers[SS1/4]==7);
  dispose_host_fixture();passed++;
  pny_setup(APP_SUCCESS,P_OK);registers[SS0/4]=measured[i];
  mutate_offset=SS0;mutate_mask=0x40;mutate_stage="CHECK stage=diagnostic-cold-state status=0";
  /* The report callback switches to the other observed value, not an invalid sentinel. */
  CHECK(pny_run()!=EFI_SUCCESS&&pny_injected);pny_no_transaction();pny_clean();
  dispose_host_fixture();passed++;
 }
 puts("PNY both measured SS0 baselines and immutable per-entry latch PASS");
 /* Full app success and original file/input/security failure paths. */
 const enum app_case app_cases[]={APP_SUCCESS,APP_ESCAPE,APP_TIMEOUT,APP_OLD_KEY,APP_OWNER,APP_SECUREBOOT,
  APP_DUPLICATE,APP_BAD_BAR,APP_NO_USB,APP_SHORT_WRITE,APP_FLUSH,APP_BAD_SIZE,APP_BAD_HASH,
  APP_BAD_PROM,APP_WATCHDOG,APP_CONFIG,APP_BME,APP_DIRTY,APP_UNMAP,APP_SELECTOR,APP_LOG_DIRTY,
  APP_FINAL_LOG,APP_CLOSE,APP_FWSEC_NO_WPR,APP_FWSEC_PIO_UNAVAILABLE,APP_FWSEC_DIAG_LOG,
  APP_MASTER_ENABLE_ERROR,APP_MASTER_DISABLE_ERROR};
 for(UINTN i=0;i<sizeof(app_cases)/sizeof(app_cases[0]);i++){
  pny_setup(app_cases[i],P_OK);EFI_STATUS s=pny_run();pny_common();
  if(app_cases[i]==APP_SUCCESS||app_cases[i]==APP_FWSEC_PIO_UNAVAILABLE){
   CHECK(s==EFI_SUCCESS&&Gu.started&&Gu.quiesced);pny_clean();
   CHECK(fake.gsp_starts==1&&fake.sec2_starts==1&&fake.flrs==1&&fake.radix_bytes==0x5053000);
   CHECK(registers[SS0/4]==0x88888888&&registers[SS1/4]==7);
   CHECK(memory_enables==1&&memory_disables==1&&display_reads);
  }else if(halted){CHECK(s!=EFI_SUCCESS&&Gu.retained);if(Gu.started)CHECK(!(config[4]&4));}
  else{CHECK(s!=EFI_SUCCESS);pny_no_transaction();pny_clean();}
  printf("PNY app %s: halted=%u enables=%zu disables=%zu PASS\n",app_names[app_cases[i]],halted,memory_enables,memory_disables);
  dispose_host_fixture();passed++;
 }
 /* Port-specific negative paths. No unexpected variant may reach firmware. */
 for(int f=P_ID;f<P_COUNT;f++){
  pny_setup(APP_SUCCESS,(enum pny_fault)f);EFI_STATUS s=pny_run();pny_common();CHECK(s!=EFI_SUCCESS);
  if(f==P_FUSE){pny_no_transaction();pny_clean();CHECK(!fake.gsp_starts&&!fake.master_enables);}
  else if(f==P_DISPLAY_LATE){pny_no_transaction();CHECK(halted&&!fake.maps&&!fake.master_enables);}
  else if(f==P_DISPLAY_FINAL){CHECK(halted&&Gu.started&&fake.flrs==1);}
  else if(f==P_LOG_RESTORE){CHECK(halted&&Gu.started&&memory_disables==1&&!PnyDecodePending);}
  else if(f==P_DISABLE_ERROR||f==P_RESTORE_CONFIG){CHECK(halted&&Gu.started);}
  else{
   pny_no_transaction();
   if(f==P_ENABLE_EXTRA||f==P_ENABLE_CONFIG||f==P_ATTR_GET){CHECK(halted||!memory_enables);}
   else if(f==P_COMMAND){CHECK(!memory_enables&&!halted&&config[4]==2&&!fake.maps);}
   else if(f==P_ATTR_DIRTY){CHECK(!memory_enables&&!halted&&fake.pci_attributes==EFI_PCI_IO_ATTRIBUTE_MEMORY);}
   else pny_clean();
  }
  printf("PNY guard %s: halted=%u enables=%zu disables=%zu PASS\n",pny_names[f],halted,memory_enables,memory_disables);
  dispose_host_fixture();passed++;
 }
 /* Retained real core error injection, with actual PNY firmware image/layout. */
 for(int m=SUCCESS;m<SCENARIO_COUNT;m++){
  pny_setup(APP_SUCCESS,P_OK);fake.mode=(enum scenario)m;
  if(m==CORE_ENTRY_SS0)registers[SS0/4]^=1;
  if(m==CORE_ENTRY_SS1)registers[SS1/4]^=1;
  if(m==DISPLAY_CHANGED){modes[0].FrameBufferBase+=4096; /* handled separately below */ dispose_host_fixture();continue;}
  EFI_STATUS s=pny_run();pny_common();
  if(m==SUCCESS||m==FWSEC_RUNNING||m==PIO_UNAVAILABLE){CHECK(s==EFI_SUCCESS);pny_clean();}
  else if(!Gu.started){CHECK(s!=EFI_SUCCESS);pny_no_transaction();pny_clean();}
  else{CHECK(s!=EFI_SUCCESS&&halted&&Gu.retained&&Gu.allocations);CHECK(!(config[4]&4));}
  printf("PNY core %s: starts=%zu/%zu flr=%zu halted=%u PASS\n",case_names[m],fake.gsp_starts,fake.sec2_starts,fake.flrs,halted);
  dispose_host_fixture();passed++;
 }
 /* Selector changes in real report callbacks, including after all app checks. */
 const char *stages[]={"CHECK stage=diagnostic-cold-state status=0","CHECK stage=final-prom status=0",
  "ARMED: one transaction","entry: plm=","selector intent:","PCIe FLR begin:",
  "Falcon reset begin: base=0x110000","Falcon DMA: base=0x110000",
  "Falcon reset begin: base=0x840000","Falcon DMA: base=0x840000","post-FLR SEC2 reset released"};
 for(UINTN sel=0;sel<2;sel++)for(UINTN i=0;i<sizeof(stages)/sizeof(stages[0]);i++){
  pny_setup(APP_SUCCESS,P_OK);mutate_stage=stages[i];mutate_offset=sel?SS1:SS0;
  CHECK(pny_run()!=EFI_SUCCESS&&pny_injected);pny_common();
  CHECK(fake.mmio_writes==writes_at_mutation);
  if(Gu.started){CHECK(halted&&Gu.retained&&Gu.allocations&&!(config[4]&4));}
  else{pny_no_transaction();pny_clean();}
  printf("PNY selector %s at %s: halted=%u PASS\n",sel?"SS1":"SS0",stages[i],halted);
  dispose_host_fixture();passed++;
 }
 for(UINTN i=0;i<5;i++)free((VOID *)originals[i].firmware.data);
 printf("%u PNY actual app/core scenarios passed; all hardware and firmware execution SIMULATED.\n",passed);
 return 0;
#endif
}
