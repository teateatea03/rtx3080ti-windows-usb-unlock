/* Modified 2026-10-25: PNY/B550 manual USB port; see docs/CHANGES-PNY.md. */ /* GPL-2.0-only. Local PNY/B550/RTX5080 experiment; no generalized fallback. */
static EFI_STATUS ReadConfig(UINT32 *Config);
static EFI_STATUS BarCheck(VOID);
typedef struct {
  EFI_HANDLE pci,gop,console,agent;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *text;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *console_interface;
  EFI_PHYSICAL_ADDRESS framebuffer;
  UINTN framebuffer_bytes;
  UINT32 number,width,height,stride,format;
} PNY_DISPLAY;
static PNY_DISPLAY PnyDisplay;
static BOOLEAN PnyDisplayLatched,PnyDecodePending;
static UINT32 PnyOriginalConfig[64];
static UINT64 PnyOriginalAttributes;
static EFI_PCI_IO_PROTOCOL *PnyDecodePci;
static EFI_GUID PnyTextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
#ifdef GU_EXPERIMENTAL_3090
static BOOLEAN PnyObservedSs0(UINT32 Value){return Value==GU_PNY_ENTRY_SS0||Value==GU_PNY_ENTRY_SS0_ALT;}
#endif
static VOID PnyReset(VOID) {
  memset(&PnyDisplay,0,sizeof(PnyDisplay));PnyDisplayLatched=FALSE;
  PnyDecodePending=FALSE;PnyDecodePci=NULL;PnyOriginalAttributes=0;
}
static BOOLEAN PnyPath(EFI_HANDLE h,const UINT8 **out,UINTN *length) {
  EFI_DEVICE_PATH_PROTOCOL *dp=NULL;*out=NULL;*length=0;
  if(EFI_ERROR(Gu.bs->HandleProtocol(h,&PathGuid,(VOID **)&dp))||!dp)return FALSE;
  const UINT8 *p=(const UINT8 *)dp;UINTN at=0;
  for(UINTN i=0;i<128&&at+4<=4096;i++){
    UINTN n=p[at+2]+((UINTN)p[at+3]<<8);
    if(n<4||n>4096-at)return FALSE;
    if(p[at]==END_DEVICE_PATH_TYPE){
      if(p[at+1]!=END_ENTIRE_DEVICE_PATH_SUBTYPE||n!=4||!at)return FALSE;
      *out=p;*length=at;return TRUE;
    }
    if(p[at]==HARDWARE_DEVICE_PATH&&p[at+1]==HW_PCI_DP&&n!=6)return FALSE;
    at+=n;
  }
  return FALSE;
}
/* Read metadata only; no output/log callbacks after the final topology sample. */
static BOOLEAN PnyDisplayCheck(VOID) {
  PNY_DISPLAY state;memset(&state,0,sizeof(state));EFI_HANDLE *hs=NULL;UINTN n=0;
  const UINT8 *gop_path=NULL,*pci_path=NULL;UINTN gn=0,pn=0;
  if(!Table->ConsoleOutHandle||Table->StandardErrorHandle!=Table->ConsoleOutHandle||!Table->ConOut)return FALSE;
  EFI_STATUS s=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&n,&hs);
  if(EFI_ERROR(s)||!hs||n!=1){if(hs)GuFree(hs);return FALSE;}
  state.gop=hs[0];GuFree(hs);hs=NULL;
  if(!PnyPath(state.gop,&gop_path,&gn))return FALSE;
  s=Gu.bs->HandleProtocol(state.gop,&GopGuid,(VOID **)&state.graphics);
  if(EFI_ERROR(s)||!state.graphics||!state.graphics->Mode)return FALSE;
  state.mode=state.graphics->Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info=state.mode->Info;
  if(!info||state.mode->SizeOfInfo<sizeof(*info)||!state.mode->FrameBufferBase||
    !state.mode->FrameBufferSize||info->PixelFormat>PixelBlueGreenRedReserved8BitPerColor||
    !info->HorizontalResolution||!info->VerticalResolution||info->PixelsPerScanLine<info->HorizontalResolution||
    (UINT64)info->PixelsPerScanLine*info->VerticalResolution>state.mode->FrameBufferSize/4)return FALSE;
  state.framebuffer=state.mode->FrameBufferBase;state.framebuffer_bytes=state.mode->FrameBufferSize;
  state.number=state.mode->Mode;state.width=info->HorizontalResolution;state.height=info->VerticalResolution;
  state.stride=info->PixelsPerScanLine;state.format=info->PixelFormat;
  s=Gu.bs->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&n,&hs);
  if(EFI_ERROR(s)||!hs||n>256){if(hs)GuFree(hs);return FALSE;}
  UINTN displays=0,matches=0;BOOLEAN target_path_valid=FALSE;
  for(UINTN i=0;i<n;i++){
    EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 c[16]={0};UINTN seg,bus,dev,fun;
    if(EFI_ERROR(Gu.bs->HandleProtocol(hs[i],&PciGuid,(VOID **)&io))||!io||
       EFI_ERROR(io->Pci.Read(io,EfiPciIoWidthUint32,0,16,c))){GuFree(hs);return FALSE;}
    if((c[2]>>24)!=3)continue;
    displays++;
    if(!PnyPath(hs[i],&pci_path,&pn)){GuFree(hs);return FALSE;}
    BOOLEAN parent=pn<=gn&&!memcmp(pci_path,gop_path,pn);
    if(hs[i]==Gu.handle){target_path_valid=!parent;continue;}
    if(!parent)continue;
    if(EFI_ERROR(io->GetLocation(io,&seg,&bus,&dev,&fun))||seg||bus!=8||dev||fun||
       c[0]!=0x2c0210deU||c[11]!=0x176219daU||c[4]!=0xe8000000U||(c[1]&7)!=7){GuFree(hs);return FALSE;}
    state.pci=hs[i];matches++;
  }
  GuFree(hs);
  if(displays!=2||matches!=1||!target_path_valid)return FALSE;
  s=Gu.bs->HandleProtocol(state.gop,&PnyTextGuid,(VOID **)&state.text);
  if(EFI_ERROR(s)||!state.text)return FALSE;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *es=NULL;UINTN count=0,links=0;
  s=Gu.bs->OpenProtocolInformation(state.gop,&PnyTextGuid,&es,&count);
  if(EFI_ERROR(s)||count>32||(count&&!es)){if(es)GuFree(es);return FALSE;}
  for(UINTN i=0;i<count;i++)
    if(es[i].Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER&&es[i].ControllerHandle==Table->ConsoleOutHandle&&
       es[i].AgentHandle&&es[i].OpenCount==1){state.agent=es[i].AgentHandle;links++;}
  if(es)GuFree(es);
  if(links!=1)return FALSE;
  state.console=Table->ConsoleOutHandle;state.console_interface=Table->ConOut;
  if(!PnyDisplayLatched){PnyDisplay=state;PnyDisplayLatched=TRUE;return TRUE;}
  return !memcmp(&PnyDisplay,&state,sizeof(state));
}
static EFI_STATUS PnyInitialCheck(VOID) {
  UINT32 config[64],normalized[64];GU_CAPS caps;UINTN seg,bus,dev,fun;
  if(!Gu.pci||EFI_ERROR(Gu.pci->GetLocation(Gu.pci,&seg,&bus,&dev,&fun))||
     EFI_ERROR(ReadConfig(config)))return EFI_DEVICE_ERROR;
  /* A separate, explicit discovery contract: initial Command MUST be zero.
     The CPU-only view then applies every ordinary runtime capability guard. */
  if((config[1]&0xffff)!=0)return EFI_UNSUPPORTED;
  memcpy(normalized,config,sizeof(config));normalized[1]|=2;
  if(!GuCheckConfig(normalized,seg,bus,dev,fun,&caps)||config[2]!=0x030000a1U||
     config[4]!=GU_BAR0_BASE||config[0xc8/4]!=0x00050011U||
     (config[(caps.pcie+8)/4]&0x00200000U))return EFI_UNSUPPORTED;
  memcpy(PnyOriginalConfig,config,sizeof(config));return EFI_SUCCESS;
}
static EFI_STATUS PnyEnableDecode(VOID) {
  UINT32 current[64],expected[64];UINT64 supported=0,after=0;
  if(GU_PREFLIGHT_ONLY||PnyDecodePending||!Gu.owned||!Gu.display_ok||!GuLogReady()||
     !Gu.pci||!Gu.pci->Attributes||Gu.started||Gu.allocations||Gu.dma_count)return EFI_ACCESS_DENIED;
  EFI_STATUS s=ReadConfig(current);
  if(EFI_ERROR(s)||memcmp(current,PnyOriginalConfig,sizeof(current))||!PnyDisplayCheck())return EFI_NOT_READY;
  s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&supported);
  if(EFI_ERROR(s)||!(supported&EFI_PCI_IO_ATTRIBUTE_MEMORY))return EFI_UNSUPPORTED;
  s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&PnyOriginalAttributes);
  if(EFI_ERROR(s)||(PnyOriginalAttributes&(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER)))return EFI_UNSUPPORTED;
  GuLogText("PNY decode intent: exact initial Command=0; enable MEMORY only; BME remains off.\n");
  if(!GuLogReady()||Gu.fault||EFI_ERROR(ReadConfig(current))||
     memcmp(current,PnyOriginalConfig,sizeof(current))||!PnyDisplayCheck())return EFI_NOT_READY;
  PnyDecodePci=Gu.pci;PnyDecodePending=TRUE;
  s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
  if(EFI_ERROR(s))return s;
  s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&after);
  if(EFI_ERROR(s)||after!=(PnyOriginalAttributes|EFI_PCI_IO_ATTRIBUTE_MEMORY))return EFI_DEVICE_ERROR;
  s=ReadConfig(current);memcpy(expected,PnyOriginalConfig,sizeof(expected));expected[1]|=2;
  if(EFI_ERROR(s)||memcmp(current,expected,sizeof(current)))return EFI_COMPROMISED_DATA;
  return EFI_SUCCESS;
}
#ifdef GU_EXPERIMENTAL_3090
/* The ARMED file/console callback is external; sample again without logging. */
static BOOLEAN PnyFinalHardware(VOID) {
  UINT32 config[64],entry0=0,entry1=0;
  if(ReadConfig(config)!=EFI_SUCCESS||memcmp(config,InitialConfig,sizeof(config))||
     !Gu.owned||!Gu.profile_verified||!Gu.prom_verified||Gu.fault||
     !GuGetEntrySelectors(&entry0,&entry1)||!PnyObservedSs0(entry0)||entry1!=GU_PNY_ENTRY_SS1)return FALSE;
  return GuRead32(0)==GU_CHIP_ID&&GuRead32(0x1183a4)==GU_DEVICE_MEMORY_SIZE&&
    GuRead32(GU_GFW_PLM_OFFSET)==0x8b8f&&GuRead32(GU_GFW_BOOT_OFFSET)==0x3ff&&
    GuRead32(GU_GFW_CPUCTL_OFFSET)==0x10&&GuRead32(0x1fa828)==0&&
    GuRead32(GU_FEAT_PLM_OFFSET)==0xffffff8f&&GuRead32(GU_FEAT_SS0_OFFSET)==entry0&&
    GuRead32(GU_FEAT_SS1_OFFSET)==entry1&&GuRead32(0x8241e0)==3&&GuRead32(0x824148)==1&&
    !Gu.fault&&PnyDisplayCheck()&&GuLogReady();
}
#endif
static EFI_STATUS PnyRestoreDecode(BOOLEAN Attempted) {
  UINT32 config[64],expected[64];UINT16 command=0xffff;UINT64 after=~0ULL;
  if(!PnyDecodePending)return EFI_SUCCESS;
  if(Gu.pci!=PnyDecodePci||!GuSafeToReturn()||Gu.allocations||Gu.dma_count||Gu.master_attributes_pending)return EFI_ACCESS_DENIED;
  /* Raw reads remain available after a latched failure; do not clear Gu.fault. */
  EFI_STATUS s=Gu.pci->Pci.Read(Gu.pci,EfiPciIoWidthUint16,4,1,&command);
  if(EFI_ERROR(s)||command==0xffff||(command&~2U))return EFI_DEVICE_ERROR;
  EFI_STATUS disabled=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationDisable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
  s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&after);
  EFI_STATUS cfg=Gu.pci->Pci.Read(Gu.pci,EfiPciIoWidthUint32,0,64,config);
  if(EFI_ERROR(disabled)||EFI_ERROR(s)||EFI_ERROR(cfg)||after!=PnyOriginalAttributes||(config[1]&0xffff))return EFI_DEVICE_ERROR;
  memcpy(expected,PnyOriginalConfig,sizeof(expected));
  if(Attempted){
    /* Only read-only PCI/PCIe status fields may legitimately change at FLR. */
    config[1]=(config[1]&0xffff)|(expected[1]&0xffff0000U);
    UINT8 *c=(UINT8 *)config,*e=(UINT8 *)expected;
    const UINTN offsets[]={0x82,0x83,0x8a,0x8b};
    for(UINTN i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++)c[offsets[i]]=e[offsets[i]];
  }
  if(memcmp(config,expected,sizeof(config))||!PnyDisplayCheck())return EFI_COMPROMISED_DATA;
  PnyDecodePending=FALSE;
  GuLogText("PNY decode restored: original protocol attributes and Command=0 verified.\n");
  return EFI_SUCCESS;
}
