/* SPDX-License-Identifier: MIT. PCI configuration and protocol metadata only. */
static UINT16 Le16(CONST UINT8 *p) { return (UINT16)((UINT16)p[0] | ((UINT16)p[1]<<8)); }
static UINT32 Le32(CONST UINT8 *p) {
    return (UINT32)p[0] | ((UINT32)p[1]<<8) | ((UINT32)p[2]<<16) | ((UINT32)p[3]<<24);
}
/* Never follows device-provided pointers outside this 256-byte host snapshot. */
static VOID ConfigCapabilities(CONST UINT8 *cfg) {
    UINT8 seen[64]={0}; UINTN count=0,at=cfg[0x34];
    Field(" CONFIG_HEADER_TYPE=",cfg[0x0e]&0x7f);
    Field(" CAP_LIST_PRESENT=",(Le16(cfg+6)&0x10)!=0); End();
    if ((cfg[0x0e]&0x7f)!=0) { Error("display-header-type",EFI_UNSUPPORTED); return; }
    if (!(Le16(cfg+6)&0x10)) {
        Text(" CAP_CHAIN=ABSENT"); Field(" ignored-pointer=",at); End(); return;
    }
    while (at) {
        if (at<0x40 || at>0xfc || (at&3) || seen[at/4] || count>=48) {
            Error("capability-pointer-or-cycle",EFI_COMPROMISED_DATA); return;
        }
        seen[at/4]=1; ++count;
        UINT8 id=cfg[at],next=cfg[at+1];
        Field(" CAP offset=",at); Field(" id=",id); Field(" next=",next);
        Field(" control-or-flags=",Le16(cfg+at+2)); End();
        if (id==1) {
            if (at>0xf8) { Error("pm-capability-bounds",EFI_COMPROMISED_DATA); return; }
            Field(" PM_CSR=",Le16(cfg+at+4)); Field(" power-state=",Le16(cfg+at+4)&3); End();
        } else if (id==5) {
            Field(" MSI_ENABLED=",Le16(cfg+at+2)&1); End();
        } else if (id==0x11) {
            Field(" MSIX_ENABLED=",(Le16(cfg+at+2)>>15)&1);
            Field(" function-mask=",(Le16(cfg+at+2)>>14)&1); End();
        } else if (id==0x10) {
            if (at>0xec) { Error("pcie-capability-bounds",EFI_COMPROMISED_DATA); return; }
            Field(" PCIE_FLAGS=",Le16(cfg+at+2));
            Field(" device-capabilities=",Le32(cfg+at+4));
            Field(" flr-capable=",(Le32(cfg+at+4)>>28)&1);
            Field(" device-control=",Le16(cfg+at+8));
            Field(" device-status=",Le16(cfg+at+10)); End();
            Field(" LINK_CAPABILITIES=",Le32(cfg+at+12));
            Field(" link-control=",Le16(cfg+at+16));
            Field(" link-status=",Le16(cfg+at+18)); End();
        }
        at=next;
    }
    Field(" CAP_CHAIN_COMPLETE count=",count); End();
}
static VOID ConfigSnapshot(EFI_PCI_IO_PROTOCOL *io, EFI_HANDLE handle) {
    UINT32 words[64]={0};
    EFI_STATUS s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,words);
    Field("CONFIG256 handle=",(UINTN)handle); Field(" status=",s); End();
    if (EFI_ERROR(s)) { Error("config256-read",s); return; }
    for (UINTN i=0;i<64;++i) {
        Field(" CFG_DWORD offset=",i*4); Field(" value=",words[i]); End();
    }
    ConfigCapabilities((CONST UINT8 *)words);
}
static VOID ConsoleMapping(EFI_HANDLE handle) {
    CONST UINT8 *p=NULL; UINTN len=0,best=0,matches=0; EFI_HANDLE owner=NULL; BOOLEAN usb=FALSE;
    if (Path(handle,&p,&len,&usb)) {
        for (UINTN i=0;i<DisplayCount;++i) if (Prefix(Displays[i].Path,Displays[i].Length,p,len)) {
            if (Displays[i].Length>best) {best=Displays[i].Length; owner=Displays[i].Handle; matches=1;}
            else if (Displays[i].Length==best) ++matches;
        }
    }
    Field(" TEXT_PCI_CANDIDATES=",matches); Field(" owner=",matches==1?(UINTN)owner:0); End();
}
/* Physical text handles may expose ConSplitter BY_CHILD_CONTROLLER edges even
 * when the aggregate SystemTable ConsoleOut handle has no DevicePath. This
 * records evidence; absent paths/edges never imply a safe display handoff.
 */
static VOID ConsoleInventory(VOID) {
    EFI_HANDLE *hs=NULL; UINTN n=0;
    if (EFI_ERROR(Handles(&TextOutGuid,&hs,&n))) return;
    Field("TEXT_OUT_COUNT=",n); End();
    for (UINTN i=0;i<n;++i) {
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out=NULL; EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;
        EFI_STATUS s=St->BootServices->HandleProtocol(hs[i],&TextOutGuid,(VOID **)&out);
        Field("TEXT_OUT handle=",(UINTN)hs[i]); Field(" status=",s);
        Field(" system-console=",hs[i]==St->ConsoleOutHandle);
        Field(" system-stderr=",hs[i]==St->StandardErrorHandle);
        if (!EFI_ERROR(s) && out && out->Mode) {
            Field(" mode=",(UINT32)out->Mode->Mode); Field(" max-mode=",(UINT32)out->Mode->MaxMode);
        }
        EFI_STATUS g=St->BootServices->HandleProtocol(hs[i],&GopGuid,(VOID **)&gop);
        Field(" direct-gop=",!EFI_ERROR(g) && gop!=NULL); End();
        PrintPath(hs[i]); ConsoleMapping(hs[i]); Ownership(hs[i],&TextOutGuid);
    }
    if (hs) St->BootServices->FreePool(hs);
    Text("CONSOLE_GRAPH_POLICY=OBSERVATION-ONLY;NO-HANDOFF-INFERENCE\r\n");
}
