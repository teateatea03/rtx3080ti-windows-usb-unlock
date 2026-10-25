/* SPDX-License-Identifier: MIT.
 * Independent bounded observation transaction. Register meanings/addresses:
 * Windows-GPU-UNLOCK e3f03cd9 internal-boot/unlock/app.c ColdCheck,
 * vendor/gfw_eval.h, prep_610.h and unlock_610.c fuse selection.
 * No firmware, selector, engine, reset or bus-master writes.
 */
#define PROBE_PROM_BYTES (1024U*1024U)
static BOOLEAN ProbeRestoreFailed,ProbeActive;
static EFI_PCI_IO_PROTOCOL *ProbeIo;
static CONST UINT32 ProbeOffsets[]={0,0x1183a4,0x118128,0x118234,0x110100,
    0x823804,0x82381c,0x823820,0x1fa824,0x1fa828,0x824148,0x8241e0};
static BOOLEAN ProbeConfig(CONST UINT32 cfg[64]) {
    CONST UINT8 *c=(CONST UINT8 *)cfg;
    return cfg[0]==0x220810deU && cfg[11]==0x1384196eU &&
        (cfg[2]>>24)==3 && (c[0x0e]&0x7f)==0 && Le16(c+4)==0 &&
        cfg[4]==0xed000000U && !(cfg[12]&1) && (Le16(c+6)&0x10) &&
        c[0x34]==0x60 && Le16(c+0x60)==0x6801 && Le16(c+0x62)==0x4803 &&
        !(Le16(c+0x64)&3) && Le16(c+0x68)==0x7805 && Le16(c+0x6a)==0x80 &&
        Le16(c+0x78)==0xb410 && Le16(c+0x7a)==0x12 &&
        (Le32(c+0x7c)&0x10000000U) && !(Le16(c+0x80)&0x8000) &&
        Le16(c+0xb4)==9 && c[0xb6]==20;
}
static BOOLEAN ProbeNoOwner(EFI_HANDLE h) {
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *es=NULL;UINTN n=0;
    EFI_STATUS s=St->BootServices->OpenProtocolInformation(h,&PciGuid,&es,&n);
    BOOLEAN ok=!EFI_ERROR(s) && n<=4096 && (!n || es);
    if(ok) for(UINTN i=0;i<n;++i)
        if(es[i].Attributes&(EFI_OPEN_PROTOCOL_BY_DRIVER|EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER))ok=FALSE;
    if(es)St->BootServices->FreePool(es);return ok;
}
static BOOLEAN ProbeBar(EFI_PCI_IO_PROTOCOL *io) {
    VOID *r=NULL;UINT64 supports=0;BOOLEAN ok=FALSE;
    EFI_STATUS s=io->GetBarAttributes(io,0,&supports,&r);
    if(!EFI_ERROR(s)&&r) {
        CONST UINT8 *p=(CONST UINT8 *)r;
        ok=p[0]==0x8a && Le16(p+1)==43 && p[3]==0 &&
            Le64(p+14)==0xed000000ULL && Le64(p+22)==0xedffffffULL &&
            Le64(p+38)==0x1000000ULL;
    }
    if(r)St->BootServices->FreePool(r);return ok;
}
static BOOLEAN ProbeConsole(EFI_HANDLE target) {
    EFI_HANDLE *hs=NULL;UINTN n=0;BOOLEAN ok=FALSE;
    if(DisplayCount!=2 || !St->ConsoleOutHandle || St->StandardErrorHandle!=St->ConsoleOutHandle)return FALSE;
    EFI_STATUS s=Handles(&GopGuid,&hs,&n);
    if(EFI_ERROR(s))return FALSE;
    if(n==1 && hs) {
        CONST UINT8 *p=NULL;UINTN len=0,matches=0;BOOLEAN usb=FALSE;EFI_HANDLE owner=NULL;
        if(Path(hs[0],&p,&len,&usb))for(UINTN i=0;i<DisplayCount;++i)
            if(Prefix(Displays[i].Path,Displays[i].Length,p,len)){++matches;owner=Displays[i].Handle;}
        EFI_PCI_IO_PROTOCOL *display=NULL;UINT32 header[16]={0};UINTN seg=0,bus=0,dev=0,fun=0;
        if(matches==1 && owner!=target &&
            !EFI_ERROR(St->BootServices->HandleProtocol(owner,&PciGuid,(VOID **)&display)) && display &&
            !EFI_ERROR(display->Pci.Read(display,EfiPciIoWidthUint32,0,16,header)) &&
            !EFI_ERROR(display->GetLocation(display,&seg,&bus,&dev,&fun)) &&
            seg==0 && bus==8 && dev==0 && fun==0 && header[0]==0x2c0210deU &&
            header[11]==0x176219daU && (header[1]&7)==7) {
            EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out=NULL;
            EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *es=NULL;UINTN entries=0,links=0;
            s=St->BootServices->HandleProtocol(hs[0],&TextOutGuid,(VOID **)&out);
            if(!EFI_ERROR(s) && out) {
                s=St->BootServices->OpenProtocolInformation(hs[0],&TextOutGuid,&es,&entries);
                if(!EFI_ERROR(s) && entries<=4096 && (!entries || es))
                    for(UINTN i=0;i<entries;++i)
                        if((es[i].Attributes&EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) &&
                            es[i].AgentHandle && es[i].ControllerHandle==St->ConsoleOutHandle)++links;
                ok=!EFI_ERROR(s) && links==1;
            }
            if(es)St->BootServices->FreePool(es);
        }
    }
    if(hs)St->BootServices->FreePool(hs);return ok;
}
static BOOLEAN ProbeRecheck(EFI_HANDLE h,EFI_PCI_IO_PROTOCOL *io,UINT32 cfg[64]) {
    UINTN seg=0,bus=0,dev=0,fun=0;EFI_PCI_IO_PROTOCOL *fresh=NULL;
    if(EFI_ERROR(St->BootServices->HandleProtocol(h,&PciGuid,(VOID **)&fresh)) || fresh!=io ||
       !io || !io->Pci.Read || !io->Pci.Write || !io->Mem.Read || !io->GetBarAttributes)return FALSE;
    return !EFI_ERROR(io->GetLocation(io,&seg,&bus,&dev,&fun)) && seg==0 && bus==4 && dev==0 && fun==0 &&
        !EFI_ERROR(io->Pci.Read(io,EfiPciIoWidthUint32,0,64,cfg)) && ProbeConfig(cfg) &&
        ProbeBar(io) && ProbeNoOwner(h) && ProbeConsole(h);
}
static EFI_STATUS ProbeWriteCommand(UINT16 value) {
    if(!ProbeActive || !ProbeIo || (value!=0 && value!=2))return EFI_ACCESS_DENIED;
    /* Exact 16-bit write, never the neighbouring W1C PCI Status register. */
    return ProbeIo->Pci.Write(ProbeIo,EfiPciIoWidthUint16,4,1,&value);
}
static EFI_STATUS ProbeRead(UINT32 offset,UINTN words,VOID *buffer) {
    BOOLEAN allowed=FALSE;
    if(!ProbeActive || !ProbeIo || !buffer)return EFI_ACCESS_DENIED;
    if(words==1)for(UINTN i=0;i<sizeof(ProbeOffsets)/sizeof(ProbeOffsets[0]);++i)
        if(offset==ProbeOffsets[i])allowed=TRUE;
    if(words==1024 && offset>=0x300000 && offset<=0x3ff000 && !(offset&0xfff))allowed=TRUE;
    if(!allowed)return EFI_ACCESS_DENIED;
    return ProbeIo->Mem.Read(ProbeIo,EfiPciIoWidthUint32,0,offset,words,buffer);
}
/* Query only: never mutate cached PCI attributes in this diagnostic. */
static BOOLEAN ProbeAttributes(EFI_PCI_IO_PROTOCOL *io) {
    UINT64 supported=0,current=~0ULL;
    if(!io->Attributes){Text("PROBE_ATTRIBUTES=MISSING\r\n");return FALSE;}
    EFI_STATUS a=io->Attributes(io,EfiPciIoAttributeOperationSupported,0,&supported);
    EFI_STATUS b=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&current);
    Field("PROBE_ATTRIBUTE_SUPPORTED_STATUS=",a);Field(" mask=",supported);End();
    Field("PROBE_ATTRIBUTE_GET_STATUS=",b);Field(" mask=",current);End();
    return a==EFI_SUCCESS && b==EFI_SUCCESS &&
        (supported&(EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER))==
        (EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER) &&
        !(current&(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER));
}
static EFI_STATUS ProbeOnce(EFI_HANDLE image) {
    EFI_HANDLE *hs=NULL,target=NULL;EFI_PCI_IO_PROTOCOL *io=NULL;UINTN n=0,found=0;
    UINT32 before[64]={0},after[64]={0},values[sizeof(ProbeOffsets)/sizeof(ProbeOffsets[0])]={0};
    VOID *prom=NULL;UINTN preIndex=0,promIndex=0;UINT16 command=0xffff;
    EFI_STATUS status=EFI_NOT_READY,s;
    BOOLEAN promReady=FALSE,restored=FALSE;UINTN regCount=0;
    ProbeActive=FALSE;ProbeIo=NULL;
    if(Problems || Truncated || EFI_ERROR(Handles(&PciGuid,&hs,&n)))return EFI_NOT_READY;
    for(UINTN i=0;i<n;++i) {
        EFI_PCI_IO_PROTOCOL *candidate=NULL;UINT32 cfg[16]={0};
        if(!EFI_ERROR(St->BootServices->HandleProtocol(hs[i],&PciGuid,(VOID **)&candidate)) &&
           candidate && !EFI_ERROR(candidate->Pci.Read(candidate,EfiPciIoWidthUint32,0,16,cfg)) &&
           cfg[0]==0x220810deU && cfg[11]==0x1384196eU){++found;target=hs[i];io=candidate;}
    }
    if(hs)St->BootServices->FreePool(hs);
    if(found!=1 || !ProbeRecheck(target,io,before) || !ProbeAttributes(io)) {
        Text("PROBE_REFUSED=TARGET-STATE-OR-DISPLAY-GUARD\r\n");return EFI_NOT_READY;
    }
    s=St->BootServices->AllocatePool(EfiBootServicesData,PROBE_PROM_BYTES,&prom);
    if(EFI_ERROR(s)||!prom)return EFI_OUT_OF_RESOURCES;
    Text("PROBE_PREFLIGHT=READY;NEXT=COMMAND-0-TO-2;BME=OFF;RESTORE-TO-0\r\n");
    s=SaveRecord(image,"PRE","TXT",Log,Used,&preIndex);
    Field("PROBE_PRE_LOG_STATUS=",s);Field(" pre-index=",preIndex);End();
    if(EFI_ERROR(s)){status=s;goto done;}
    /* Filesystem callbacks may change firmware state. Revalidate after them. */
    if(!ProbeRecheck(target,io,before)||!ProbeAttributes(io)){status=EFI_NOT_READY;goto done;}
    ProbeIo=io;ProbeActive=TRUE;
    /* A failed write may still have affected hardware: cleanup is mandatory. */
    status=ProbeWriteCommand(2);
    if(EFI_ERROR(status))goto restore;
    status=io->Pci.Read(io,EfiPciIoWidthUint16,4,1,&command);
    if(EFI_ERROR(status)||command!=2){status=EFI_DEVICE_ERROR;goto restore;}
    for(UINTN i=0;i<sizeof(ProbeOffsets)/sizeof(ProbeOffsets[0]);++i) {
        status=ProbeRead(ProbeOffsets[i],1,&values[i]);
        if(EFI_ERROR(status))goto restore;
        ++regCount;
        if(values[i]==0xffffffffU || (values[i]&0xffff0000U)==0xbadf0000U){status=EFI_DEVICE_ERROR;goto restore;}
        if(i==0 && (values[i]&0xfff00000U)!=0xb7200000U){status=EFI_UNSUPPORTED;goto restore;}
    }
    for(UINT32 offset=0;offset<PROBE_PROM_BYTES;offset+=4096) {
        status=ProbeRead(0x300000+offset,1024,(UINT8 *)prom+offset);
        if(EFI_ERROR(status))goto restore;
    }
    promReady=TRUE;
restore:
    for(UINTN attempt=0;attempt<2 && !restored;++attempt) {
        s=ProbeWriteCommand(0);
        EFI_STATUS readback=io->Pci.Read(io,EfiPciIoWidthUint16,4,1,&command);
        /* A write error remains a diagnostic failure even if state restored. */
        if(EFI_ERROR(s) && !EFI_ERROR(status))status=s;
        restored=!EFI_ERROR(readback) && command==0;
    }
    ProbeActive=FALSE;ProbeIo=NULL;
    Field("PROBE_RESTORE_VERIFIED=",restored);Field(" command=",command);End();
    if(!restored){ProbeRestoreFailed=TRUE;status=EFI_DEVICE_ERROR;Error("probe-restore-failed",status);}
    else {
        s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,after);
        BOOLEAN equal=!EFI_ERROR(s);
        if(equal)for(UINTN i=0;i<64;++i)if(before[i]!=after[i])equal=FALSE;
        Field("PROBE_CONFIG256_UNCHANGED=",equal);End();
        if(!equal && !EFI_ERROR(status))status=EFI_COMPROMISED_DATA;
    }
    for(UINTN i=0;i<regCount;++i){Field("PROBE_REG offset=",ProbeOffsets[i]);Field(" value=",values[i]);End();}
    if(promReady && restored) {
        s=SaveRecord(image,"PRM","BIN",prom,PROBE_PROM_BYTES,&promIndex);
        Field("PROBE_PROM_SAVE_STATUS=",s);Field(" prom-index=",promIndex);Field(" bytes=",PROBE_PROM_BYTES);End();
        if(EFI_ERROR(s))status=s;
    }
done:
    St->BootServices->FreePool(prom);
    return status;
}
