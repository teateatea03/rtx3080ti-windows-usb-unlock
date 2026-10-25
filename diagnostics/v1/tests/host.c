/* SPDX-License-Identifier: MIT. Executes the actual EFI entry against fake protocols. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* EDK2 supplies its own NULL; keep the firmware headers unmodified. */
#undef NULL
#define DIAG_HOST_TEST
#include "../src/diagnostic.c"

static EFI_BOOT_SERVICES Bs;
static EFI_SYSTEM_TABLE Table;
static EFI_PCI_IO_PROTOCOL Gpu[2];
static EFI_LOADED_IMAGE_PROTOCOL Image;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL Fs;
static EFI_FILE_PROTOCOL Root,Marker,File;
static UINT8 Paths[3][16];
static int Usb,MarkerValid,Existing,Full,ShortWrite,FlushError,HeaderError;
static int NoPci,OverLimit,PartialPath;
static int Opens,Creates,Writes,Closes,Passed;
static CHAR8 Captured[LOG_CAP];
static CHAR16 WrittenName[64];
#define H(n) ((EFI_HANDLE)(UINTN)(n))
#define CHECK(c) do {if (!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);exit(1);}} while(0)
static int Eq(EFI_GUID *a,EFI_GUID *b) {return memcmp(a,b,sizeof(*a))==0;}
static EFI_STATUS EFIAPI Alloc(EFI_MEMORY_TYPE type,UINTN n,VOID **p) {
    (void)type;*p=malloc(n);return *p?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI Free(VOID *p) {free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Enumerate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **p) {
    (void)type;(void)key;*p=NULL;*n=0;
    if (Eq(g,&PciGuid)) {if(NoPci)return EFI_NOT_FOUND;*n=OverLimit?4097:2;}
    else if (Eq(g,&GopGuid)) *n=2;
    else return EFI_NOT_FOUND;
    *p=malloc(*n*sizeof(EFI_HANDLE));CHECK(*p!=NULL);
    for(UINTN i=0;i<*n;++i)(*p)[i]=H(Eq(g,&PciGuid)?i+1:i+3);
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Protocol(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
    *p=NULL;
    if(Eq(g,&PciGuid)&&(h==H(1)||h==H(2)))*p=&Gpu[h==H(2)];
    else if(Eq(g,&PathGuid)) {
        if(h==H(1)||h==H(3))*p=Paths[0];
        else if(h==H(2)||h==H(4))*p=Paths[1];
        else if(h==H(7))*p=Paths[2];
    } else if(Eq(g,&ImageGuid)&&h==H(6))*p=&Image;
    else if(Eq(g,&FsGuid)&&h==H(7))*p=&Fs;
    return *p?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Owners(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **p,UINTN *n) {
    (void)h;(void)g;*p=NULL;*n=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Config(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *p) {
    CHECK(w==EfiPciIoWidthUint32);CHECK(off<0x100);CHECK(n<=16);
    if(HeaderError&&off==0)return EFI_DEVICE_ERROR;
    memset(p,0,n*4);
    if(off==0) {UINT32 *a=p;a[0]=io==&Gpu[0]?0x220810de:0x2c0210de;a[2]=0x030000a1;a[11]=io==&Gpu[0]?0x1384196e:0x176219da;}
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
    *seg=0;*bus=io==&Gpu[0]?4:8;*dev=0;*fun=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Bar(EFI_PCI_IO_PROTOCOL *io,UINT8 bar,UINT64 *support,VOID **resource) {
    (void)io;CHECK(bar<6);*support=0;*resource=NULL;return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f) {(void)f;++Closes;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *buf) {
    CHECK(f==&Marker);CONST CHAR8 *s=MarkerValid?"GPU-DIAG-USB-LOG-v1":"GPU-DIAG-USB-LOG-v1-extra";
    *n=strlen(s);memcpy(buf,s,*n);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *buf) {
    CHECK(f==&File);++Writes;CHECK(*n<LOG_CAP);memcpy(Captured,buf,*n);Captured[*n]=0;
    if(ShortWrite)--*n;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Flush(EFI_FILE_PROTOCOL *f) {CHECK(f==&File);return FlushError?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *root,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attr) {
    CHECK(root==&Root);(void)attr;++Opens;*out=NULL;
    if(name[10]=='A') {CHECK(mode==EFI_FILE_MODE_READ);*out=&Marker;return EFI_SUCCESS;}
    CHECK(name[10]=='R'&&name[13]>='0'&&name[16]<='9');
    if(mode==EFI_FILE_MODE_READ) {
        if(Full||Existing>0){if(!Full)--Existing;*out=&File;return EFI_SUCCESS;}
        return EFI_NOT_FOUND;
    }
    CHECK(mode==(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE));
    ++Creates;for(UINTN i=0;i<64;++i){WrittenName[i]=name[i];if(!name[i])break;}
    *out=&File;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *f,EFI_FILE_PROTOCOL **root) {
    CHECK(f==&Fs);*root=&Root;return EFI_SUCCESS;
}
static VOID Setup(VOID) {
    memset(&Bs,0,sizeof(Bs));memset(&Table,0,sizeof(Table));memset(Gpu,0,sizeof(Gpu));
    memset(&Root,0,sizeof(Root));memset(&Marker,0,sizeof(Marker));memset(&File,0,sizeof(File));
    memset(Paths,0,sizeof(Paths));memset(Captured,0,sizeof(Captured));memset(WrittenName,0,sizeof(WrittenName));
    Usb=1;MarkerValid=1;Existing=0;Full=0;ShortWrite=0;FlushError=0;HeaderError=0;NoPci=0;OverLimit=0;PartialPath=0;
    Opens=0;Creates=0;Writes=0;Closes=0;
    Bs.AllocatePool=Alloc;Bs.FreePool=Free;Bs.HandleProtocol=Protocol;
    Bs.LocateHandleBuffer=Enumerate;Bs.OpenProtocolInformation=Owners;
    Table.BootServices=&Bs;Table.ConsoleOutHandle=H(5);Image.DeviceHandle=H(7);Fs.OpenVolume=Volume;
    Root.Open=Open;Root.Close=Close;Marker.Read=Read;Marker.Close=Close;File.Write=Write;File.Flush=Flush;File.Close=Close;
    for(UINTN i=0;i<2;++i){Gpu[i].Pci.Read=Config;Gpu[i].GetLocation=Location;Gpu[i].GetBarAttributes=Bar;
        Paths[i][0]=1;Paths[i][1]=1;Paths[i][2]=6;Paths[i][5]=(UINT8)(4+4*i);Paths[i][6]=0x7f;Paths[i][7]=0xff;Paths[i][8]=4;}
    Paths[2][0]=3;Paths[2][1]=5;Paths[2][2]=6;Paths[2][6]=0x7f;Paths[2][7]=0xff;Paths[2][8]=4;
}
static VOID Run(CONST char *name) {CHECK(efi_main(H(6),&Table)==EFI_SUCCESS);++Passed;printf("PASS %s\n",name);}
int main(VOID) {
    /* Given: dual GPUs. When: real entry runs. Then: unique USB report maps both GOPs. */
    Setup();Run("dual_gpu_inventory");CHECK(Writes==1&&Creates==1);CHECK(strstr(Captured,"id=00000000220810de"));
    CHECK(strstr(Captured,"id=000000002c0210de"));CHECK(strstr(Captured,"GOP_PCI_CANDIDATES=0000000000000001"));
    CHECK(strstr(Captured,"END-OF-INVENTORY"));CHECK(WrittenName[16]=='0');
    /* Given: internal launch volume. When: entry runs. Then: no filesystem is opened. */
    Setup();Paths[2][1]=1;Run("internal_volume_never_opened");CHECK(Opens==0&&Writes==0);
    /* Given: a malformed USB node. When: entry runs. Then: no disk is opened. */
    Setup();Paths[2][2]=4;Run("malformed_usb_never_opened");CHECK(Opens==0&&Writes==0);
    /* Given: marker with extra bytes. When: entry runs. Then: no file is created. */
    Setup();MarkerValid=0;Run("invalid_marker_rejected");CHECK(Creates==0&&Writes==0);
    /* Given: an existing report. When: entry runs. Then: next filename is selected. */
    Setup();Existing=1;Run("existing_log_not_overwritten");CHECK(Creates==1&&WrittenName[16]=='1');
    /* Given: all names occupied. When: save runs. Then: no write and explicit failure. */
    Setup();Full=1;St=&Table;Log=malloc(LOG_CAP);Used=0;Log[0]=0;
    CHECK(SaveLog(H(6))==EFI_VOLUME_FULL&&Creates==0&&Writes==0);free(Log);Log=NULL;++Passed;puts("PASS log_namespace_full");
    /* Given: short disk write. When: save runs. Then: it cannot report success. */
    Setup();ShortWrite=1;St=&Table;Log=malloc(LOG_CAP);Used=4;memcpy(Log,"test",5);
    CHECK(SaveLog(H(6))==EFI_DEVICE_ERROR);free(Log);Log=NULL;++Passed;puts("PASS short_write_detected");
    /* Given: flush failure. When: save runs. Then: it cannot report success. */
    Setup();FlushError=1;St=&Table;Log=malloc(LOG_CAP);Used=4;memcpy(Log,"test",5);
    CHECK(SaveLog(H(6))==EFI_DEVICE_ERROR);free(Log);Log=NULL;++Passed;puts("PASS flush_failure_detected");
    /* Given: config read error. When: entry runs. Then: report preserves the errors. */
    Setup();HeaderError=1;Run("pci_read_failure_reported");CHECK(strstr(Captured,"ERROR pci-header"));
    /* Given: no PCI protocol handles. When: entry runs. Then: absence remains explicit. */
    Setup();NoPci=1;Run("no_pci_reported");CHECK(strstr(Captured,"PCI_HANDLE_COUNT=0000000000000000"));
    /* Given: excessive firmware handle count. When: entry runs. Then: processing is bounded. */
    Setup();OverLimit=1;Run("handle_limit_rejected");CHECK(strstr(Captured,"ERROR handle-count"));
    /* Given: malformed and multi-instance paths. When: parser runs. Then: neither is accepted. */
    Setup();UINTN len=0;BOOLEAN usb=FALSE;Paths[0][2]=3;CHECK(!PathLength(Paths[0],&len,&usb));
    Paths[0][2]=6;Paths[0][7]=1;CHECK(!PathLength(Paths[0],&len,&usb));++Passed;puts("PASS invalid_paths_rejected");
    /* Given: nearly full output. When: text is appended. Then: overflow is detected and bounded. */
    Setup();Log=malloc(LOG_CAP);Used=LOG_CAP-2;Truncated=FALSE;Text("abcd");CHECK(Truncated&&Used==LOG_CAP-1&&Log[Used]==0);
    free(Log);Log=NULL;++Passed;puts("PASS output_overflow_detected");
    printf("%d scenarios passed; host fixtures are not hardware validation.\n",Passed);return 0;
}
