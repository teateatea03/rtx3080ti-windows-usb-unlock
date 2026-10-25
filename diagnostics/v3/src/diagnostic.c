/* SPDX-License-Identifier: MIT. Bounded diagnostic; no unlock implementation. */
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DriverBinding.h>

#define LOG_CAP (512U * 1024U)
#define MAX_PATH 4096U
#define MAX_DISPLAYS 32U
static EFI_GUID PciGuid = EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID PathGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID ImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID FsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID BindingGuid = EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID TextOutGuid = EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
static EFI_SYSTEM_TABLE *St;
static CHAR8 *Log;
static UINTN Used;
static BOOLEAN Truncated;
static UINTN Problems;
typedef struct { EFI_HANDLE Handle; CONST UINT8 *Path; UINTN Length; } DISPLAY;
static DISPLAY Displays[MAX_DISPLAYS];
static UINTN DisplayCount;
static CONST CHAR8 * volatile ProgramName="GPU-DIAG v3\r\n";

static VOID Text(CONST CHAR8 *s) {
    while (*s) {
        if (Used + 1 < LOG_CAP) Log[Used++] = *s;
        else Truncated = TRUE;
        ++s;
    }
    Log[Used] = 0;
}
static VOID Hex(UINT64 value, UINTN digits) {
    CONST CHAR8 *h = "0123456789abcdef";
    while (digits) { --digits; CHAR8 s[2] = {h[(value >> (digits * 4)) & 15],0}; Text(s); }
}
static VOID Field(CONST CHAR8 *name, UINT64 value) { Text(name); Hex(value,16); }
static VOID End(VOID) { Text("\r\n"); }
static VOID Error(CONST CHAR8 *name, EFI_STATUS status) {
    ++Problems; Text("ERROR "); Text(name); Field(" status=",status); End();
}
static UINTN NodeSize(CONST UINT8 *p) { return (UINTN)p[2] | ((UINTN)p[3] << 8); }
/* Exclude the end node; reject multi-instance and malformed/overlong paths. */
static BOOLEAN PathLength(CONST UINT8 *p, UINTN *length, BOOLEAN *usb) {
    UINTN at=0;
    *length=0; *usb=FALSE;
    if (!p) return FALSE;
    for (UINTN nodes=0; nodes<128 && at+4<=MAX_PATH; ++nodes) {
        UINTN size=NodeSize(p+at);
        if (size<4 || size>MAX_PATH-at) return FALSE;
        if (p[at]==0x7f) {
            if (p[at+1]!=0xff || size!=4) return FALSE;
            *length=at; return TRUE;
        }
        if (p[at]==1 && p[at+1]==1 && size!=6) return FALSE;
        if (p[at]==3 && p[at+1]==5) { if (size!=6) return FALSE; *usb=TRUE; }
        if (p[at]==3 && p[at+1]==15) { if (size!=11) return FALSE; *usb=TRUE; }
        if (p[at]==3 && p[at+1]==16) { if (size<10 || (size&1)) return FALSE; *usb=TRUE; }
        at+=size;
    }
    return FALSE;
}
static BOOLEAN Prefix(CONST UINT8 *a, UINTN an, CONST UINT8 *b, UINTN bn) {
    if (!an || an>bn) return FALSE;
    for (UINTN i=0;i<an;++i) if (a[i]!=b[i]) return FALSE;
    return TRUE;
}
static BOOLEAN Path(EFI_HANDLE handle, CONST UINT8 **path, UINTN *length, BOOLEAN *usb) {
    EFI_DEVICE_PATH_PROTOCOL *dp=NULL;
    EFI_STATUS s=St->BootServices->HandleProtocol(handle,&PathGuid,(VOID **)&dp);
    *path=(CONST UINT8 *)dp; *length=0; *usb=FALSE;
    if (EFI_ERROR(s) || !dp) return FALSE;
    return PathLength(*path,length,usb);
}
static VOID PrintPath(EFI_HANDLE handle) {
    CONST UINT8 *p=NULL; UINTN n=0; BOOLEAN usb=FALSE;
    Field("PATH handle=",(UINTN)handle);
    if (!Path(handle,&p,&n,&usb)) { Text(" unavailable-or-invalid"); End(); return; }
    Field(" length=",n); Field(" usb=",usb); End();
    for (UINTN at=0;at<n;at+=NodeSize(p+at)) {
        UINTN size=NodeSize(p+at);
        Text(" NODE type="); Hex(p[at],2); Text(" subtype="); Hex(p[at+1],2);
        Field(" bytes=",size);
        if (p[at]==1 && p[at+1]==1 && size==6) { Field(" device=",p[at+5]); Field(" function=",p[at+4]); }
        /* Media/USB serial and partition identifiers are intentionally omitted. */
        End();
    }
}
static EFI_STATUS Handles(EFI_GUID *guid, EFI_HANDLE **handles, UINTN *count) {
    *handles=NULL; *count=0;
    EFI_STATUS s=St->BootServices->LocateHandleBuffer(ByProtocol,guid,NULL,count,handles);
    if (s==EFI_NOT_FOUND) return EFI_SUCCESS;
    if (EFI_ERROR(s)) { Error("LocateHandleBuffer",s); return s; }
    if (*count>4096 || (*count && !*handles)) {
        if (*handles) St->BootServices->FreePool(*handles);
        *handles=NULL; *count=0; Error("handle-count",EFI_COMPROMISED_DATA); return EFI_COMPROMISED_DATA;
    }
    return EFI_SUCCESS;
}
static VOID Ownership(EFI_HANDLE handle, EFI_GUID *guid) {
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL; UINTN n=0;
    EFI_STATUS s=St->BootServices->OpenProtocolInformation(handle,guid,&entries,&n);
    Field("OWNERS handle=",(UINTN)handle); Field(" protocol-data1=",guid->Data1); Field(" status=",s); Field(" entries=",n); End();
    if (EFI_ERROR(s)) Error("ownership-read",s);
    else if (n>4096 || (n && !entries)) Error("ownership-entries",EFI_COMPROMISED_DATA);
    if (!EFI_ERROR(s) && entries && n<=4096) {
        for (UINTN i=0;i<n;++i) {
            Field(" AGENT=",(UINTN)entries[i].AgentHandle);
            Field(" controller=",(UINTN)entries[i].ControllerHandle);
            Field(" attributes=",entries[i].Attributes); Field(" count=",entries[i].OpenCount); End();
        }
    }
    if (entries) St->BootServices->FreePool(entries);
}
static UINT64 Le64(CONST UINT8 *p) {
    UINT64 n=0; for (UINTN i=0;i<8;++i) n|=(UINT64)p[i]<<(8*i); return n;
}
static VOID Bars(EFI_PCI_IO_PROTOCOL *io) {
    for (UINT8 bar=0;bar<6;++bar) {
        UINT32 value=0; VOID *resource=NULL; UINT64 supports=0;
        EFI_STATUS s=io->Pci.Read(io,EfiPciIoWidthUint32,0x10U+4U*bar,1,&value);
        Field(" BAR index=",bar); Field(" config-status=",s);
        if (!EFI_ERROR(s)) Field(" value=",value);
        s=io->GetBarAttributes(io,bar,&supports,&resource);
        Field(" resource-status=",s); Field(" supports=",supports);
        if (!EFI_ERROR(s) && resource) {
            CONST UINT8 *p=(CONST UINT8 *)resource;
            /* UEFI returns an ACPI QWord Address Space descriptor here. */
            if (p[0]==0x8a) {
                UINTN bytes=(UINTN)p[1] | ((UINTN)p[2]<<8);
                if (bytes==43) { Field(" type=",p[3]); Field(" min=",Le64(p+14)); Field(" max=",Le64(p+22)); Field(" size=",Le64(p+38)); }
                else Text(" unrecognized-descriptor-length");
            } else Text(" unrecognized-descriptor");
        }
        if (resource) St->BootServices->FreePool(resource);
        End();
    }
}
#include "extended.h"
static VOID PciInventory(VOID) {
    EFI_HANDLE *hs=NULL; UINTN n=0;
    if (EFI_ERROR(Handles(&PciGuid,&hs,&n))) return;
    Field("PCI_HANDLE_COUNT=",n); End();
    for (UINTN i=0;i<n;++i) {
        EFI_PCI_IO_PROTOCOL *io=NULL; UINT32 header[16]={0};
        EFI_STATUS s=St->BootServices->HandleProtocol(hs[i],&PciGuid,(VOID **)&io);
        if (EFI_ERROR(s) || !io) { Error("pci-protocol",s); continue; }
        s=io->Pci.Read(io,EfiPciIoWidthUint32,0,16,header);
        if (EFI_ERROR(s)) { Error("pci-header",s); continue; }
        UINT8 cls=(UINT8)(header[2]>>24);
        if (cls!=3 && cls!=6) continue;
        UINTN seg=0,bus=0,device=0,function=0;
        s=io->GetLocation(io,&seg,&bus,&device,&function);
        if (EFI_ERROR(s)) { Error("pci-location",s); continue; }
        Field("PCI handle=",(UINTN)hs[i]); Field(" segment=",seg); Field(" bus=",bus);
        Field(" device=",device); Field(" function=",function);
        Field(" id=",header[0]); Field(" class-revision=",header[2]); Field(" command-status=",header[1]);
        if (cls==3) Field(" subsystem=",header[11]);
        End(); PrintPath(hs[i]);
        if (cls==3) {
            CONST UINT8 *p=NULL; UINTN len=0; BOOLEAN usb=FALSE;
            if (DisplayCount<MAX_DISPLAYS && Path(hs[i],&p,&len,&usb)) {
                Displays[DisplayCount].Handle=hs[i]; Displays[DisplayCount].Path=p;
                Displays[DisplayCount++].Length=len;
            } else Error("display-path-or-limit",EFI_COMPROMISED_DATA);
            Bars(io); ConfigSnapshot(io,hs[i]); Ownership(hs[i],&PciGuid);
        }
    }
    if (hs) St->BootServices->FreePool(hs);
    Field("DISPLAY_PCI_COUNT=",DisplayCount); End();
}
static VOID DisplayInventory(VOID) {
    EFI_HANDLE *hs=NULL; UINTN n=0;
    if (EFI_ERROR(Handles(&GopGuid,&hs,&n))) return;
    Field("GOP_COUNT=",n); End();
    for (UINTN i=0;i<n;++i) {
        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;
        EFI_STATUS s=St->BootServices->HandleProtocol(hs[i],&GopGuid,(VOID **)&gop);
        Field("GOP handle=",(UINTN)hs[i]); Field(" status=",s);
        if (!EFI_ERROR(s) && gop && gop->Mode) {
            Field(" mode=",gop->Mode->Mode);
            if (gop->Mode->Info && gop->Mode->SizeOfInfo>=sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) {
                Field(" width=",gop->Mode->Info->HorizontalResolution); Field(" height=",gop->Mode->Info->VerticalResolution);
                Field(" format=",gop->Mode->Info->PixelFormat);
            }
        }
        End(); PrintPath(hs[i]);
        CONST UINT8 *p=NULL; UINTN len=0,best=0,matches=0; EFI_HANDLE owner=NULL; BOOLEAN usb=FALSE;
        if (Path(hs[i],&p,&len,&usb)) {
            for (UINTN j=0;j<DisplayCount;++j) if (Prefix(Displays[j].Path,Displays[j].Length,p,len)) {
                if (Displays[j].Length>best) {best=Displays[j].Length; owner=Displays[j].Handle; matches=1;}
                else if (Displays[j].Length==best) ++matches;
            }
        }
        Field(" GOP_PCI_CANDIDATES=",matches); Field(" owner=",matches==1?(UINTN)owner:0); End();
        Ownership(hs[i],&GopGuid);
    }
    if (hs) St->BootServices->FreePool(hs);
    Field("CONSOLE_OUT=",(UINTN)St->ConsoleOutHandle); End(); PrintPath(St->ConsoleOutHandle);
    Field("CONSOLE_ERR=",(UINTN)St->StandardErrorHandle); End();
    Field("CONSOLE_IN=",(UINTN)St->ConsoleInHandle); End();
}
static VOID DriverInventory(VOID) {
    EFI_HANDLE *hs=NULL; UINTN n=0;
    if (EFI_ERROR(Handles(&BindingGuid,&hs,&n))) return;
    Field("DRIVER_BINDING_COUNT=",n); End();
    for (UINTN i=0;i<n;++i) {
        EFI_DRIVER_BINDING_PROTOCOL *d=NULL;
        if (!EFI_ERROR(St->BootServices->HandleProtocol(hs[i],&BindingGuid,(VOID **)&d)) && d) {
            Field("DRIVER handle=",(UINTN)hs[i]); Field(" image=",(UINTN)d->ImageHandle); Field(" version=",d->Version); End();
        }
    }
    if (hs) St->BootServices->FreePool(hs);
}
/* Only write to the loaded image's volume after a USB path + exact marker. */
static EFI_STATUS SaveRecord(EFI_HANDLE image, CONST CHAR8 *tag, CONST CHAR8 *extension, VOID *buffer, UINTN length, UINTN *savedIndex) {
    EFI_LOADED_IMAGE_PROTOCOL *li=NULL; EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
    EFI_FILE_PROTOCOL *root=NULL,*marker=NULL,*file=NULL;
    CONST UINT8 *p=NULL; UINTN len=0; BOOLEAN usb=FALSE;
    EFI_STATUS s=St->BootServices->HandleProtocol(image,&ImageGuid,(VOID **)&li);
    if (EFI_ERROR(s) || !li || !Path(li->DeviceHandle,&p,&len,&usb) || !usb) return EFI_UNSUPPORTED;
    s=St->BootServices->HandleProtocol(li->DeviceHandle,&FsGuid,(VOID **)&fs);
    if (EFI_ERROR(s) || !fs) return EFI_UNSUPPORTED;
    s=fs->OpenVolume(fs,&root);
    if (EFI_ERROR(s) || !root) return EFI_ERROR(s)?s:EFI_DEVICE_ERROR;
    s=root->Open(root,&marker,L"\\GPU-DIAG\\ALLOW-LOG.TXT",EFI_FILE_MODE_READ,0);
    if (EFI_ERROR(s) || !marker) { root->Close(root); return EFI_ACCESS_DENIED; }
    CHAR8 token[32]={0}; UINTN bytes=sizeof(token);
    s=marker->Read(marker,&bytes,token); marker->Close(marker);
    CONST CHAR8 *expected="GPU-DIAG-USB-LOG-v1"; UINTN expectedLen=sizeof("GPU-DIAG-USB-LOG-v1")-1;
    BOOLEAN valid=!EFI_ERROR(s) && bytes==expectedLen;
    if (valid) for (UINTN i=0;i<expectedLen;++i) if (token[i]!=expected[i]) valid=FALSE;
    if (!valid) { root->Close(root); return EFI_ACCESS_DENIED; }
    CHAR16 name[]=L"\\GPU-DIAG\\RUN0000.TXT";
    for (UINTN k=0;k<3;++k) { name[10+k]=(CHAR16)tag[k]; name[18+k]=(CHAR16)extension[k]; }
    UINTN index=0;
    for (;index<10000;++index) {
        UINTN value=index;
        for (UINTN digit=0;digit<4;++digit) {name[16-digit]=(CHAR16)('0'+value%10);value/=10;}
        s=root->Open(root,&file,name,EFI_FILE_MODE_READ,0);
        if (!EFI_ERROR(s)) {
            if (!file) { s=EFI_DEVICE_ERROR; break; }
            s=file->Close(file); file=NULL;
            if (EFI_ERROR(s)) break;
            continue;
        }
        if (s!=EFI_NOT_FOUND) break;
        s=root->Open(root,&file,name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
        if (EFI_ERROR(s) || !file) { if (!EFI_ERROR(s)) s=EFI_DEVICE_ERROR; break; }
        bytes=length; s=file->Write(file,&bytes,buffer);
        if (!EFI_ERROR(s) && bytes!=length) s=EFI_DEVICE_ERROR;
        if (!EFI_ERROR(s)) s=file->Flush(file);
        EFI_STATUS closed=file->Close(file); file=NULL;
        if (!EFI_ERROR(s) && EFI_ERROR(closed)) s=closed;
        break;
    }
    if (index==10000) s=EFI_VOLUME_FULL;
    if (!EFI_ERROR(s) && savedIndex) *savedIndex=index;
    root->Close(root); return s;
}
static EFI_STATUS SaveLog(EFI_HANDLE image) { return SaveRecord(image,"RUN","TXT",Log,Used,NULL); }
#include "probe.h"
static VOID Screen(CONST CHAR8 *s) {
    if (!St->ConOut || !St->ConOut->OutputString) return;
    CHAR16 chunk[128];
    while (*s) {
        UINTN n=0; while (*s && n<127) chunk[n++]=(CHAR16)(UINT8)*s++;
        chunk[n]=0; St->ConOut->OutputString(St->ConOut,chunk);
    }
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *table) {
    St=table; Used=0; Truncated=FALSE; Problems=0; DisplayCount=0;
    if (!St || !St->BootServices) return EFI_INVALID_PARAMETER;
    EFI_STATUS s=St->BootServices->AllocatePool(EfiBootServicesData,LOG_CAP,(VOID **)&Log);
    if (EFI_ERROR(s) || !Log) return EFI_OUT_OF_RESOURCES;
    Log[0]=0;
    Screen("GPU-DIAG v3: bounded 3080 Ti diagnostic. Please wait.\r\n");
    Text(ProgramName);
    Text("POLICY=EXACT-3080TI;TEMP-MEMORY-DECODE;WHITELIST-READ;RESTORE;NO-UNLOCK;NO-RESET;NO-DMA\r\n");
    Field("UEFI_REVISION=",St->Hdr.Revision); End();
    PciInventory(); DisplayInventory(); ConsoleInventory(); DriverInventory();
    ProbeRestoreFailed=FALSE;
    EFI_STATUS probe=ProbeOnce(image); Field("PROBE_STATUS=",probe); End();
    Field("PROBLEMS=",Problems); Field(" TRUNCATED=",Truncated); End();
    Text("END-OF-INVENTORY\r\n");
    s=Truncated?EFI_BUFFER_TOO_SMALL:SaveLog(image);
    Screen(Log);
    if (EFI_ERROR(s)) Screen("USB_LOG=DISABLED-OR-FAILED (no confirmed complete log).\r\n");
    else Screen("USB_LOG=SAVED in GPU-DIAG\\RUNxxxx.TXT.\r\n");
    if (!EFI_ERROR(probe)) Screen("PROBE=COMPLETE; RESTORE=VERIFIED.\r\n");
    else Screen("PROBE=REFUSED-OR-FAILED; see final RUN log.\r\n");
    if (ProbeRestoreFailed) {
        Screen("RESTORE=FAILED: STOP. Do not boot Windows. Power off this PC.\r\n");
#ifndef DIAG_HOST_TEST
        for (;;) { if (St->BootServices->Stall) St->BootServices->Stall(1000000); }
#endif
    }
    Screen("Press any key to return to firmware. No reboot or chainload.\r\n");
#ifndef DIAG_HOST_TEST
    if (St->ConIn && St->ConIn->WaitForKey) {
        UINTN index=0; EFI_INPUT_KEY key;
        if (!EFI_ERROR(St->BootServices->WaitForEvent(1,&St->ConIn->WaitForKey,&index)))
            St->ConIn->ReadKeyStroke(St->ConIn,&key);
    }
#endif
    St->BootServices->FreePool(Log); Log=NULL;
    return EFI_SUCCESS;
}
